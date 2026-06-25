// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/protocol/bnet/fsm_auth_create_login_test.cpp
//
// Fast FSM-level coverage for the real OLS auth path:
// SID_CREATEACCTREQ1 account creation and SID_LOGONRESPONSE2 credential login,
// driven through BnetFsm with the genuine LoginUser / CreateAccount use-cases
// over in-memory repositories (no sockets, no bnetd process).
//
// These lock three things the e2e journey exercises but that no unit test did:
//   1. on(CreateAccount1Request) actually creates an account (was a no-op stub);
//   2. on(LogonResponse2) packs the 5xu32 hash1 into the 20 bytes BNHash needs,
//      so a password created on one round-trips to a successful login on the
//      other (a stringified hash would reject every login with 0x02);
//   3. a successful login does NOT double-attach the session (LoginUser already
//      attaches; a second attach in the FSM used to reject() and close a valid
//      login). We assert success is *sent* and the context is *not* closed.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/create_account.hpp"
#include "application/auth/login_user.hpp"
#include "core/clock.hpp"
#include "infra/crypto/bnet_hash.hpp"
#include "infra/crypto/bnet_session_hasher.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/ip_ban_repository.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "protocol/bnet/fsm.hpp"
#include "protocol/bnet/messages.hpp"

#include "capturing_session_context.hpp"

using namespace pvpgn;
using namespace pvpgn::protocol::bnet;
using pvpgn::protocol::bnet::test::CapturingSessionContext;

namespace {

constexpr std::array<std::uint32_t, 5> kPassword{
    0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u, 0x55555555u};
constexpr std::array<std::uint32_t, 5> kWrongPassword{
    0x66666666u, 0x66666666u, 0x66666666u, 0x66666666u, 0x66666666u};

/// Owns the in-memory collaborators and builds a BnetFsm wired with the real
/// LoginUser / CreateAccount use-cases. `with_create` toggles whether the
/// create-account use-case is present (to exercise the unwired refusal path).
struct Harness {
    infra::inmemory::InMemoryAccountRepository accounts;
    infra::inmemory::InMemorySessionRegistry   sessions;
    infra::inmemory::InMemoryEventBus          bus;
    infra::inmemory::InMemoryIpBanRepository   ip_bans;
    core::ManualClock                          clock{core::SystemTime{}};

    std::shared_ptr<CapturingSessionContext> ctx =
        std::make_shared<CapturingSessionContext>();

    infra::crypto::BnetSessionHasher hasher;

    BnetUseCaseContext make_ctx(bool with_create) {
        BnetUseCaseContext uc;
        uc.login_user = std::make_shared<application::auth::LoginUser>(
            accounts, sessions, bus, clock, hasher);
        if (with_create) {
            uc.create_account = std::make_shared<application::auth::CreateAccount>(
                accounts, ip_bans, bus, clock);
        }
        uc.account_repo = std::shared_ptr<domain::identity::IAccountRepository>(
            &accounts, [](domain::identity::IAccountRepository*) noexcept {});
        uc.session_registry = std::shared_ptr<domain::identity::ISessionRegistry>(
            &sessions, [](domain::identity::ISessionRegistry*) noexcept {});
        return uc;
    }

    BnetFsm make_fsm(bool with_create = true) {
        return BnetFsm{ctx, make_ctx(with_create), domain::SessionId{1}};
    }
};

CreateAccount1Request create_req(const char* name,
                                 std::array<std::uint32_t, 5> words = kPassword) {
    CreateAccount1Request m;
    m.password_hash1 = words;
    m.player_name    = name;
    return m;
}

/// Compute the OLS hash2 the way a real client does: the broken-SHA-1 of
/// client_token ‖ server_token ‖ hash1 (all little-endian). The resulting
/// digest words, packed LE on the wire, are exactly what the server's
/// BnetSessionHasher re-derives from the stored hash1 + tokens.
std::array<std::uint32_t, 5> compute_hash2(std::array<std::uint32_t, 5> hash1,
                                           std::uint32_t client_token,
                                           std::uint32_t server_token) {
    std::array<std::byte, 28> buf{};
    auto put_le32 = [&buf](std::size_t off, std::uint32_t v) {
        buf[off + 0] = static_cast<std::byte>(v & 0xFFu);
        buf[off + 1] = static_cast<std::byte>((v >> 8) & 0xFFu);
        buf[off + 2] = static_cast<std::byte>((v >> 16) & 0xFFu);
        buf[off + 3] = static_cast<std::byte>((v >> 24) & 0xFFu);
    };
    put_le32(0, client_token);
    put_le32(4, server_token);
    for (std::size_t i = 0; i < hash1.size(); ++i) put_le32(8 + i * 4, hash1[i]);
    return pvpgn::v3::infra::crypto::blizzard_hash(std::span<const std::byte>{buf});
}

constexpr std::uint32_t kClientToken = 0xDEADBEEFu;
constexpr std::uint32_t kServerToken = 0u;

LogonResponse2 logon_req(const char* name,
                         std::array<std::uint32_t, 5> words = kPassword) {
    LogonResponse2 m;
    m.client_token  = kClientToken;
    m.server_token  = kServerToken;
    // The client sends hash2 (the double-hash), not the stored hash1.
    m.password_hash = compute_hash2(words, kClientToken, kServerToken);
    m.username      = name;
    return m;
}

template <class T>
const T* last_as(const std::shared_ptr<CapturingSessionContext>& ctx) {
    if (ctx->all_sent().empty()) return nullptr;
    return std::get_if<T>(&ctx->all_sent().back());
}

}  // namespace

TEST_CASE("fsm auth: CREATEACCTREQ1 creates an account", "[protocol][bnet][auth]") {
    Harness h;
    auto fsm = h.make_fsm();

    REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(fsm.handle(ClientMessage{create_req("e2euser")}).has_value());

    const auto* reply = last_as<CreateAccount1Reply>(h.ctx);
    REQUIRE(reply != nullptr);
    CHECK(reply->result == kCreateAccount1ResultOk);
    // The account is now persisted in the shared repository.
    CHECK(h.accounts.find_by_name(domain::UserName::parse("e2euser").value()).has_value());
}

TEST_CASE("fsm auth: create then login succeeds (hash1 packing round-trips)",
          "[protocol][bnet][auth]") {
    Harness h;
    auto fsm = h.make_fsm();

    REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(fsm.handle(ClientMessage{create_req("e2euser")}).has_value());
    REQUIRE(fsm.handle(ClientMessage{logon_req("e2euser")}).has_value());

    const auto* reply = last_as<LogonResponse2Reply>(h.ctx);
    REQUIRE(reply != nullptr);
    CHECK(reply->result == 0x00u);          // accepted — packing matched
    CHECK_FALSE(h.ctx->closed());           // no double-attach reject/close
    // The session was attached exactly once, to this session id.
    CHECK(h.sessions.account_for(domain::SessionId{1}).has_value());
}

TEST_CASE("fsm auth: wrong password is rejected with 0x02",
          "[protocol][bnet][auth]") {
    Harness h;
    auto fsm = h.make_fsm();

    REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(fsm.handle(ClientMessage{create_req("e2euser")}).has_value());
    REQUIRE(fsm.handle(ClientMessage{logon_req("e2euser", kWrongPassword)}).has_value());

    const auto* reply = last_as<LogonResponse2Reply>(h.ctx);
    REQUIRE(reply != nullptr);
    CHECK(reply->result == 0x02u);          // InvalidCredentials
}

TEST_CASE("fsm auth: unknown account is rejected with 0x01",
          "[protocol][bnet][auth]") {
    Harness h;
    auto fsm = h.make_fsm();

    REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(fsm.handle(ClientMessage{logon_req("ghostuser")}).has_value());

    const auto* reply = last_as<LogonResponse2Reply>(h.ctx);
    REQUIRE(reply != nullptr);
    CHECK(reply->result == 0x01u);          // UnknownUser
}

TEST_CASE("fsm auth: CREATEACCTREQ1 without a use-case refuses (does not ACK)",
          "[protocol][bnet][auth]") {
    Harness h;
    auto fsm = h.make_fsm(/*with_create=*/false);

    REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(fsm.handle(ClientMessage{create_req("e2euser")}).has_value());

    const auto* reply = last_as<CreateAccount1Reply>(h.ctx);
    REQUIRE(reply != nullptr);
    CHECK(reply->result == kCreateAccount1ResultNo);
    CHECK_FALSE(h.accounts.find_by_name(
        domain::UserName::parse("e2euser").value()).has_value());
}

TEST_CASE("fsm auth: duplicate account creation is refused",
          "[protocol][bnet][auth]") {
    Harness h;
    auto fsm = h.make_fsm();

    REQUIRE(fsm.handle(ClientMessage{AuthInfo{}}).has_value());
    REQUIRE(fsm.handle(ClientMessage{create_req("e2euser")}).has_value());
    REQUIRE(fsm.handle(ClientMessage{create_req("e2euser")}).has_value());

    const auto* reply = last_as<CreateAccount1Reply>(h.ctx);
    REQUIRE(reply != nullptr);
    CHECK(reply->result == kCreateAccount1ResultNo);   // UsernameTaken
}
