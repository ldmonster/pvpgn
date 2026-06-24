// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_auth_reject_branches_test.cpp
/// Reject / edge branches of the Connecting + Authenticating handlers that the
/// existing connecting/authenticating tests (happy path + wrong-state) miss:
///
///   connection_fsm_connecting.cpp:
///     - on_auth_check out-of-order (state != Authenticating) -> silent ok
///     - on_logon_request empty-username reject (stub path) -> result 0x01, stays Connecting
///     - on_logon_request injected-OLS + UserName::parse failure -> reject reply
///
///   connection_fsm_authenticating.cpp:
///     - on_auth_accountlogon empty-username reject -> 0x53 reply with result 0x01
///     - on_auth_accountlogonproof fallback when no pending username (value_or empty)
///
/// The OLS-injection cases reuse the LoginUser-over-in-memory-repos recipe from
/// connection_fsm_connecting_usecase_test.cpp.

#include "connection_fsm_test_fixtures.hpp"

#include <optional>
#include <string>
#include <string_view>

#include "application/auth/login_user.hpp"
#include "application/auth/login_user_nls.hpp"
#include "core/clock.hpp"
#include "infra/crypto/nls_crypto_adapter.hpp"
#include "infra/inmemory/account_repository.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/session_registry.hpp"

using namespace pvpgn::test::connection_fsm;

namespace {

namespace auth  = pvpgn::application::auth;
namespace inmem = pvpgn::infra::inmemory;

// Minimal NLS credential store, only needed to satisfy the OLS+NLS ctor.
class InMemoryNlsCredentialStore final : public auth::INlsCredentialStore {
public:
    std::optional<auth::NlsCredentials> find(std::string_view) override {
        return std::nullopt;
    }
};

struct OlsEnv {
    inmem::InMemoryAccountRepository accounts;
    inmem::InMemorySessionRegistry  sessions;
    inmem::InMemoryEventBus         bus;
    pvpgn::core::SystemClock        clock;
    InMemoryNlsCredentialStore             nls_store;
    pvpgn::infra::crypto::NlsCryptoAdapter  nls_crypto;

    auth::LoginUser    ols() { return auth::LoginUser{accounts, sessions, bus, clock}; }
    auth::LoginUserNls nls() { return auth::LoginUserNls{nls_store, nls_crypto}; }
};

// Build a SID_LOGON_REQUEST with an arbitrary (possibly invalid) username,
// mirroring make_logon_request but allowing names the helper's default forbids.
std::vector<std::byte> make_logon_request_raw(std::string_view username) {
    std::vector<std::byte> p;
    push_le32(p, 0xDEADu);  // client_token
    push_le32(p, 0xBEEFu);  // server_token
    for (int i = 0; i < 5; ++i) push_le32(p, 0u);  // password_hash
    push_cstr(p, username);
    return p;
}

}  // namespace

// --- on_auth_check out of order (before Authenticating) is a silent ok -------

TEST_CASE("ConnectionFsm: SID_AUTH_CHECK in Connecting is silently ignored",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_auth_check();
    auto result  = fsm.dispatch(sid::kAuthCheck,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());   // out-of-order is ignored, not rejected
    CHECK(fsm.state() == ConnectionState::Connecting);  // unchanged
    CHECK_FALSE(ctx.closed);
    CHECK(ctx.sent.empty());       // no reply on the ignored path
}

// --- on_logon_request empty username (stub path) ----------------------------

TEST_CASE("ConnectionFsm: SID_LOGON_REQUEST with empty username is refused",
          "[connection_fsm][connecting]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};

    auto payload = make_logon_request_raw("");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());                       // sends a 0x01 result, no throw
    CHECK(fsm.state() == ConnectionState::Connecting);  // not logged in
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kLogonRequest);
    REQUIRE(ctx.last()->payload.size() >= 4);
    CHECK(ctx.last()->payload[0] == std::byte{0x01});  // result = invalid password
}

// --- on_logon_request injected OLS + invalid username -> reject reply --------

TEST_CASE("ConnectionFsm: injected OLS rejects a username that fails UserName::parse",
          "[connection_fsm][connecting][usecase]") {
    OlsEnv env;
    auto ols = env.ols();
    auto nls = env.nls();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, ols, nls};
    ctx.sent.clear();

    // "bad name" is non-empty (passes the empty check) but contains a space,
    // so UserName::parse fails inside on_logon_request's OLS block.
    auto payload = make_logon_request_raw("bad name");
    auto result  = fsm.dispatch(sid::kLogonRequest,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() != ConnectionState::LoggedIn);
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kLogonRequest);
    REQUIRE(ctx.last()->payload.size() >= 4);
    CHECK(ctx.last()->payload[0] == std::byte{0x01});  // bad username -> 0x01
}

// --- on_auth_accountlogon empty username -> 0x53 reply with result 0x01 ------

TEST_CASE("ConnectionFsm: SID_AUTH_ACCOUNTLOGON with empty username is refused",
          "[connection_fsm][authenticating]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_authenticating(fsm);
    ctx.sent.clear();

    auto payload = make_accountlogon("");  // empty username after the 32-byte key
    auto result  = fsm.dispatch(sid::kAuthAccountLogon,
                                std::span<const std::byte>{payload});

    REQUIRE(result.has_value());
    CHECK(fsm.state() == ConnectionState::Authenticating);  // no transition
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthAccountLogon);
    REQUIRE(ctx.last()->payload.size() >= 4);
    CHECK(ctx.last()->payload[0] == std::byte{0x01});  // account does not exist
}

// --- on_auth_accountlogonproof fallback with no pending username -------------
// A bare PROOF in Authenticating (no preceding ACCOUNTLOGON, no NLS use-case)
// hits the fallback that sets username_ = pending_nls_username_.value_or("").

TEST_CASE("ConnectionFsm: PROOF without a prior ACCOUNTLOGON uses the empty-username fallback",
          "[connection_fsm][authenticating]") {
    FakeContext ctx;
    ConnectionFsm fsm{ctx};
    reach_authenticating(fsm);
    ctx.sent.clear();

    auto proof = make_accountlogonproof();
    auto result = fsm.dispatch(sid::kAuthAccountLogonProof,
                               std::span<const std::byte>{proof});

    REQUIRE(result.has_value());
    // Fallback accepts: transitions to LoggedIn with placeholder account, empty name.
    CHECK(fsm.state() == ConnectionState::LoggedIn);
    CHECK(fsm.username().empty());
    REQUIRE(ctx.last() != nullptr);
    CHECK(ctx.last()->packet_id == sid::kAuthAccountLogonProof);
}
