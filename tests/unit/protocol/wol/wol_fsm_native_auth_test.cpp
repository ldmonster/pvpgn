// SPDX-License-Identifier: GPL-2.0-or-later
/// @file wol_fsm_native_auth_test.cpp
/// Unit tests for the NATIVE Westwood Online auth path in WolFsm: the
/// CVERS / VERCHK / APGAR / NICK / USER handshake wired via WolAuthDeps
/// (CreateAccount + IWolCredentialStore), mirroring the original server.
///
///   - VERCHK replies 379 NONREQ (and echoes the SKU)
///   - first login auto-creates the account, stores the APGAR, sends MOTD (376)
///   - re-login with the same APGAR is accepted (376)
///   - re-login with a different APGAR is rejected (378), stays unauthenticated
///   - USER without APGAR does not authenticate
///   - missing params → 461

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/create_account.hpp"
#include "core/clock.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/inmemory/wol_credential_store.hpp"
#include "protocol/wol/wol_fsm.hpp"
#include "protocol/wol/wol_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::wol;

namespace {

class FakeWolCtx : public IWolSessionContext {
public:
    std::vector<std::string> lines;
    bool                     closed = false;

    core::Status<> send_line(std::string_view line) override {
        lines.emplace_back(line);
        return core::ok();
    }
    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        std::string buf(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        std::size_t pos = 0;
        while (pos < buf.size()) {
            auto crlf = buf.find("\r\n", pos);
            if (crlf == std::string::npos) { lines.push_back(buf.substr(pos)); break; }
            lines.push_back(buf.substr(pos, crlf - pos));
            pos = crlf + 2;
        }
        return core::ok();
    }
    void close() override { closed = true; }
    std::string_view server_name() const noexcept override { return "wol.test"; }

    [[nodiscard]] bool has(std::string_view needle) const {
        for (const auto& l : lines)
            if (l.find(needle) != std::string::npos) return true;
        return false;
    }
};

std::span<const std::byte> as_bytes(std::string_view s) {
    return {reinterpret_cast<const std::byte*>(s.data()), s.size()};
}
core::Status<> feed(WolFsm& fsm, std::string_view line) {
    std::string buf(line);
    buf += "\r\n";
    return fsm.on_bytes(as_bytes(buf));
}

struct WolFixture {
    infra::inmemory::InMemoryAccountRepository   accounts;
    infra::inmemory::InMemorySessionRegistry     sessions;
    infra::inmemory::InMemoryEventBus            bus;
    infra::inmemory::InMemoryIpBanRepository     ip_bans;
    infra::inmemory::InMemoryWolCredentialStore  wol_store;
    core::ManualClock                            clock{core::SystemTime{}};
    application::auth::CreateAccount create_account{accounts, ip_bans, bus, clock};

    WolAuthDeps deps() {
        return WolAuthDeps{&create_account, &accounts, &wol_store, &sessions};
    }

    // Drive the full WOL handshake for (user, apgar). Caller owns the ctx.
    void handshake(WolFsm& fsm, std::string_view user, std::string_view apgar,
                   int sku = 1000) {
        REQUIRE(feed(fsm, std::string("CVERS 1 ") + std::to_string(sku)).has_value());
        REQUIRE(feed(fsm, std::string("VERCHK ") + std::to_string(sku) + " 1.0").has_value());
        REQUIRE(feed(fsm, std::string("APGAR ") + std::string(apgar)).has_value());
        REQUIRE(feed(fsm, std::string("NICK ") + std::string(user)).has_value());
        REQUIRE(feed(fsm, std::string("USER ") + std::string(user) +
                          " HostName irc.westwood.com :Real").has_value());
    }
};

}  // namespace

TEST_CASE("WolFsm native: VERCHK replies 379 and echoes the SKU",
          "[protocol][wol][fsm][wolauth]") {
    WolFixture f;
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx, f.deps()};
    REQUIRE(feed(fsm, "CVERS 1 4096").has_value());
    REQUIRE(feed(fsm, "VERCHK 4096 1.0").has_value());
    CHECK(ctx->has(" 379 "));
    CHECK(ctx->has("4096"));
    CHECK(ctx->has("NONREQ"));
}

TEST_CASE("WolFsm native: first login auto-creates the account and sends MOTD",
          "[protocol][wol][fsm][wolauth]") {
    WolFixture f;
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx, f.deps()};

    f.handshake(fsm, "wolnewbie", "tokenAAA");

    CHECK(fsm.state() == WolState::Authenticated);
    CHECK(ctx->has(" 375 "));
    CHECK(ctx->has(" 376 "));
    CHECK_FALSE(ctx->closed);
    // Account now exists and the APGAR token is stored.
    CHECK(f.accounts.find_by_name(domain::UserName::parse("wolnewbie").value()));
    auto creds = f.wol_store.find("wolnewbie");
    REQUIRE(creds.has_value());
    CHECK(creds->apgar == "tokenAAA");
}

TEST_CASE("WolFsm native: re-login with the same APGAR is accepted",
          "[protocol][wol][fsm][wolauth]") {
    WolFixture f;
    {
        auto ctx = std::make_shared<FakeWolCtx>();
        WolFsm fsm{ctx, f.deps()};
        f.handshake(fsm, "wolrepeat", "secretTok");
        REQUIRE(fsm.state() == WolState::Authenticated);
    }
    auto ctx2 = std::make_shared<FakeWolCtx>();
    WolFsm fsm2{ctx2, f.deps()};
    f.handshake(fsm2, "wolrepeat", "secretTok");
    CHECK(fsm2.state() == WolState::Authenticated);
    CHECK(ctx2->has(" 376 "));
    CHECK_FALSE(ctx2->has(" 378 "));
}

TEST_CASE("WolFsm native: re-login with a wrong APGAR is rejected (378)",
          "[protocol][wol][fsm][wolauth]") {
    WolFixture f;
    {
        auto ctx = std::make_shared<FakeWolCtx>();
        WolFsm fsm{ctx, f.deps()};
        f.handshake(fsm, "wolwrong", "rightTok");
        REQUIRE(fsm.state() == WolState::Authenticated);
    }
    auto ctx2 = std::make_shared<FakeWolCtx>();
    WolFsm fsm2{ctx2, f.deps()};
    f.handshake(fsm2, "wolwrong", "WRONGTok");
    CHECK(ctx2->has(" 378 "));
    CHECK(fsm2.state() != WolState::Authenticated);
}

TEST_CASE("WolFsm native: USER without APGAR does not authenticate",
          "[protocol][wol][fsm][wolauth]") {
    WolFixture f;
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx, f.deps()};
    REQUIRE(feed(fsm, "CVERS 1 1000").has_value());
    REQUIRE(feed(fsm, "NICK noapgar").has_value());
    REQUIRE(feed(fsm, "USER noapgar HostName irc.westwood.com :Real").has_value());
    CHECK(fsm.state() != WolState::Authenticated);
    CHECK_FALSE(ctx->has(" 376 "));
}

TEST_CASE("WolFsm native: missing params yield 461",
          "[protocol][wol][fsm][wolauth]") {
    WolFixture f;
    auto ctx = std::make_shared<FakeWolCtx>();
    WolFsm fsm{ctx, f.deps()};
    REQUIRE(feed(fsm, "CVERS").has_value());
    REQUIRE(feed(fsm, "APGAR").has_value());
    CHECK(ctx->has(" 461 "));
}
