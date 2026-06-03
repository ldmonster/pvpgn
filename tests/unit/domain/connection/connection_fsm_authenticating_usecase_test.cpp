// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm_authenticating_usecase_test.cpp
/// Covers the injected-LoginUserNls paths of connection_fsm_authenticating.cpp —
/// the `if (login_user_nls_ != nullptr)` blocks in on_account_logon (challenge)
/// and on_account_logon_proof (verify). The other authenticating tests drive
/// only the stub fallback (placeholder account_id_). Wires a real LoginUserNls
/// over an in-memory NLS credential store + the production NlsCryptoAdapter.

#include "connection_fsm_test_fixtures.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>

#include "application/auth/login_user_nls.hpp"
#include "infra/crypto/nls.hpp"
#include "infra/crypto/nls_crypto_adapter.hpp"
#include "infra/crypto/nls_verifier.hpp"

using namespace pvpgn::test::connection_fsm;

namespace {

using pvpgn::application::auth::INlsCredentialStore;
using pvpgn::application::auth::LoginUserNls;
using pvpgn::application::auth::NlsCredentials;

// Minimal in-memory INlsCredentialStore (case-insensitive), mirroring the one
// in tests/.../login_user_nls_test.cpp.
class InMemoryNlsCredentialStore final : public INlsCredentialStore {
public:
    void insert(std::string username, NlsCredentials creds) {
        for (auto& c : username)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        store_[std::move(username)] = std::move(creds);
    }

    std::optional<NlsCredentials> find(std::string_view username) override {
        std::string key{username};
        for (auto& c : key)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        auto it = store_.find(key);
        if (it == store_.end()) return std::nullopt;
        return it->second;
    }

private:
    std::unordered_map<std::string, NlsCredentials> store_;
};

// Fixture: one registered account "alice" with a real SRP (salt, verifier).
struct NlsEnv {
    InMemoryNlsCredentialStore       store;
    pvpgn::infra::crypto::NlsCryptoAdapter crypto;

    NlsEnv() {
        auto [salt, verifier] =
            pvpgn::infra::crypto::NlsVerifier::create_verifier("alice", "secret");
        NlsCredentials creds;
        creds.salt     = salt;
        creds.verifier = verifier;
        store.insert("alice", creds);
    }

    LoginUserNls use_case() { return LoginUserNls{store, crypto}; }
};

}  // namespace

TEST_CASE("ConnectionFsm: injected LoginUserNls runs the challenge path",
          "[connection_fsm][authenticating][usecase]") {
    NlsEnv env;
    auto nls = env.use_case();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, nls};
    reach_authenticating(fsm);   // AUTH_INFO -> Authenticating
    ctx.sent.clear();

    auto logon = make_accountlogon("alice");
    auto result = fsm.dispatch(sid::kAuthAccountLogon,
                               std::span<const std::byte>{logon});

    // challenge() succeeds for the known account; the FSM sends an
    // SID_AUTH_ACCOUNTLOGON reply carrying salt + server public key.
    REQUIRE(result.has_value());
    CHECK_FALSE(ctx.sent.empty());
    CHECK_FALSE(ctx.closed);
}

TEST_CASE("ConnectionFsm: injected LoginUserNls rejects an unknown account",
          "[connection_fsm][authenticating][usecase]") {
    NlsEnv env;
    auto nls = env.use_case();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, nls};
    reach_authenticating(fsm);
    ctx.sent.clear();

    auto logon = make_accountlogon("nobody");  // not in the store
    auto result = fsm.dispatch(sid::kAuthAccountLogon,
                               std::span<const std::byte>{logon});

    // The FSM handles the lookup failure (an error reply or a clean close);
    // either way it must not authenticate. (No throw is asserted implicitly.)
    (void)result;
    CHECK(fsm.state() != ConnectionState::LoggedIn);
}

TEST_CASE("ConnectionFsm: injected LoginUserNls verify with a bad proof fails",
          "[connection_fsm][authenticating][usecase]") {
    NlsEnv env;
    auto nls = env.use_case();

    FakeContext ctx;
    ConnectionFsm fsm{ctx, nls};
    reach_authenticating(fsm);

    // Challenge first so the FSM holds NLS context for the proof step.
    auto logon = make_accountlogon("alice");
    REQUIRE(fsm.dispatch(sid::kAuthAccountLogon,
                         std::span<const std::byte>{logon}).has_value());
    ctx.sent.clear();

    // A canned (incorrect) client proof must not authenticate.
    auto proof = make_accountlogonproof();
    auto result = fsm.dispatch(sid::kAuthAccountLogonProof,
                               std::span<const std::byte>{proof});

    (void)result;
    CHECK(fsm.state() != ConnectionState::LoggedIn);   // wrong proof -> not in
}
