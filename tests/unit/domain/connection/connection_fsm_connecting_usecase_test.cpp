// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_connecting_usecase_test.cpp
/// Covers the injected-LoginUser (OLS) path of connection_fsm_connecting.cpp —
/// the `if (login_user_ols_ != nullptr)` block in on_logon_request. The other
/// connecting tests drive only the stub fallback (placeholder account_id_).
/// Wires a real application::auth::LoginUser over in-memory repos and drives the
/// OLS SID_LOGON_REQUEST success / wrong-password / unknown-account branches.
///
/// The FSM's OLS+NLS constructor needs both use-cases, so a LoginUserNls is also
/// constructed (over an in-memory SRP store) but never exercised here.

#include "connection_fsm_test_fixtures.hpp"

#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>

#include "application/auth/login_user.hpp"
#include "application/auth/login_user_nls.hpp"
#include "core/clock.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/crypto/nls.hpp"
#include "infra/crypto/nls_crypto_adapter.hpp"
#include "infra/crypto/nls_verifier.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/session_registry.hpp"

using namespace pvpgn::test::connection_fsm;

namespace {

namespace auth  = pvpgn::application::auth;
namespace inmem = pvpgn::infra::inmemory;

// --- minimal NLS credential store, only to satisfy the OLS+NLS ctor ---------
class InMemoryNlsCredentialStore final : public auth::INlsCredentialStore {
public:
    std::optional<auth::NlsCredentials> find(std::string_view) override {
        return std::nullopt;
    }
};

// Seed an account with the given id, name, and password hash.
void seed(inmem::InMemoryAccountRepository& repo, std::uint32_t id,
          std::string_view name, pvpgn::domain::BNHash hash) {
    auto uname = pvpgn::domain::UserName::parse(name).value();
    auto acc = pvpgn::domain::identity::Account::rehydrate(
        pvpgn::domain::AccountId{id}, uname, hash,
        pvpgn::domain::Locale::parse_or_default("enUS"),
        pvpgn::domain::identity::CommandGroupMask{}, std::nullopt, false);
    REQUIRE(repo.save(acc));
}

pvpgn::domain::BNHash nonzero_hash() {
    pvpgn::domain::BNHash::Bytes b{};
    b.fill(static_cast<pvpgn::domain::BNHash::Bytes::value_type>(0x5A));
    return pvpgn::domain::BNHash{b};
}

// Bundle the OLS use-case dependencies so each test can build a fresh FSM.
struct OlsEnv {
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry  sessions;
    inmem::InMemoryEventBus         bus;
    pvpgn::core::SystemClock        clock;

    // NLS plumbing (unused by these OLS cases, required by the ctor).
    InMemoryNlsCredentialStore         nls_store;
    pvpgn::infra::crypto::NlsCryptoAdapter nls_crypto;

    auth::LoginUser    ols()  { return auth::LoginUser{accounts, sessions, bus, clock}; }
    auth::LoginUserNls nls()  { return auth::LoginUserNls{nls_store, nls_crypto}; }
};

}  // namespace

TEST_CASE("ConnectionFsm: injected OLS LoginUser accepts a valid login",
          "[connection_fsm][connecting][usecase]") {
    OlsEnv env;
    // make_logon_request sends an all-zero password hash; a default BNHash is
    // also all zeros, so this account's stored hash matches.
    seed(env.accounts, 7, "bob", pvpgn::domain::BNHash{});

    auto ols = env.ols();
    auto nls = env.nls();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, ols, nls};
    // on_logon_request runs from the initial Connecting state (legacy OLS:
    // no AUTH_INFO first), so dispatch SID_LOGON_REQUEST directly.
    ctx.sent.clear();

    auto payload = make_logon_request("bob");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.account_id() == 7u);  // real id from the use-case, not placeholder
    CHECK_FALSE(ctx.sent.empty());
}

TEST_CASE("ConnectionFsm: injected OLS LoginUser rejects a wrong password",
          "[connection_fsm][connecting][usecase]") {
    OlsEnv env;
    // Stored hash is non-zero, but the request sends zeros -> mismatch.
    seed(env.accounts, 7, "bob", nonzero_hash());

    auto ols = env.ols();
    auto nls = env.nls();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, ols, nls};
    ctx.sent.clear();

    auto payload = make_logon_request("bob");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());                       // FSM replies, doesn't throw
    CHECK(fsm.state() != ConnectionState::LoggedIn);    // not authenticated
}

TEST_CASE("ConnectionFsm: injected OLS LoginUser rejects an unknown account",
          "[connection_fsm][connecting][usecase]") {
    OlsEnv env;  // no accounts seeded

    auto ols = env.ols();
    auto nls = env.nls();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, ols, nls};
    ctx.sent.clear();

    auto payload = make_logon_request("ghost");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() != ConnectionState::LoggedIn);
}
