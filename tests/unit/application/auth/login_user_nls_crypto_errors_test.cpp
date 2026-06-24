// SPDX-License-Identifier: GPL-2.0-or-later
//
// Coverage for `application::auth::LoginUserNls::verify()`'s error
// mapping. login_user_nls_test.cpp drives challenge() + the InvalidProof
// path against the *real* crypto adapter, but never reaches the
// NlsCryptoError::CryptoError or NlsCryptoError::InvalidPublicKey arms of
// the verify() switch (the real adapter returns InvalidProof for a bad M1).
//
// Here we inject a hand-rolled INlsCryptoService fake whose verify_proof()
// can be steered to return each NlsCryptoError variant, pinning the
// use-case's error-translation table:
//   NlsCryptoError::CryptoError      -> NlsLoginError::CryptoError
//   NlsCryptoError::InvalidPublicKey -> NlsLoginError::InvalidProof
//   NlsCryptoError::InvalidProof     -> NlsLoginError::InvalidProof
// plus the success path (verify_proof returns M2 -> NlsProofResult).

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/login_user_nls.hpp"
#include "application/auth/nls_crypto.hpp"
#include "core/result.hpp"

namespace {

using namespace pvpgn;
using application::auth::INlsCredentialStore;
using application::auth::INlsCryptoService;
using application::auth::LoginUserNls;
using application::auth::NlsCredentials;
using application::auth::NlsCryptoContext;
using application::auth::NlsCryptoError;
using application::auth::NlsLoginError;
using application::auth::NlsProofResult;

// ---------------------------------------------------------------------------
// In-memory credential store (always returns a fixed account).
// ---------------------------------------------------------------------------

class InMemoryNlsCredentialStore final : public INlsCredentialStore {
public:
    void insert(std::string username, NlsCredentials creds) {
        store_[std::move(username)] = std::move(creds);
    }
    std::optional<NlsCredentials> find(std::string_view username) override {
        auto it = store_.find(std::string{username});
        if (it == store_.end()) return std::nullopt;
        return it->second;
    }

private:
    std::unordered_map<std::string, NlsCredentials> store_;
};

// ---------------------------------------------------------------------------
// Steerable crypto fake: verify_proof() returns either a fixed M2 or a
// caller-selected NlsCryptoError.
// ---------------------------------------------------------------------------

class FakeCrypto final : public INlsCryptoService {
public:
    std::optional<NlsCryptoError> fail_with;  // nullopt => succeed
    std::array<std::byte, 20>     m2{};       // returned on success

    NlsCryptoContext create_challenge(
        std::string_view /*username*/,
        std::span<const std::byte> /*verifier*/,
        std::span<const std::byte> stored_salt) override {
        NlsCryptoContext ctx;
        // Copy the salt through so the use-case threads it into the result.
        std::size_t n = std::min(ctx.salt.size(), stored_salt.size());
        for (std::size_t i = 0; i < n; ++i) ctx.salt[i] = stored_salt[i];
        ctx.server_public_key.fill(std::byte{0xAB});
        return ctx;
    }

    core::Result<std::array<std::byte, 20>, NlsCryptoError>
    verify_proof(NlsCryptoContext& /*ctx*/,
                 std::string_view /*username*/,
                 std::span<const std::byte> /*A*/,
                 std::span<const std::byte> /*M1*/) override {
        if (fail_with) return core::fail(*fail_with);
        return core::Result<std::array<std::byte, 20>, NlsCryptoError>(m2);
    }
};

struct Fixture {
    InMemoryNlsCredentialStore store;
    FakeCrypto                 crypto;
    domain::AccountId          acct{7};

    Fixture() {
        NlsCredentials creds;
        creds.salt.fill(std::byte{0x11});
        creds.verifier.fill(std::byte{0x22});
        creds.account_id = acct;
        store.insert("alice", creds);
    }

    LoginUserNls make() { return LoginUserNls{store, crypto}; }
};

std::array<std::byte, 128> any_A() {
    std::array<std::byte, 128> a{};
    a.fill(std::byte{0x02});
    return a;
}
std::array<std::byte, 20> any_M1() {
    std::array<std::byte, 20> m{};
    m.fill(std::byte{0x03});
    return m;
}

}  // namespace

TEST_CASE("LoginUserNls::verify maps NlsCryptoError::CryptoError to "
          "NlsLoginError::CryptoError",
          "[application][auth][nls]") {
    Fixture f;
    f.crypto.fail_with = NlsCryptoError::CryptoError;
    auto uc = f.make();

    auto ch = uc.challenge("alice", any_A());
    REQUIRE(ch);

    auto A = any_A();
    auto M1 = any_M1();
    auto r = uc.verify("alice", ch.value().crypto_ctx, A, M1, f.acct);

    REQUIRE_FALSE(r);
    CHECK(r.error() == NlsLoginError::CryptoError);
}

TEST_CASE("LoginUserNls::verify maps NlsCryptoError::InvalidPublicKey to "
          "NlsLoginError::InvalidProof",
          "[application][auth][nls]") {
    Fixture f;
    f.crypto.fail_with = NlsCryptoError::InvalidPublicKey;
    auto uc = f.make();

    auto ch = uc.challenge("alice", any_A());
    REQUIRE(ch);

    auto A = any_A();
    auto M1 = any_M1();
    auto r = uc.verify("alice", ch.value().crypto_ctx, A, M1, f.acct);

    REQUIRE_FALSE(r);
    CHECK(r.error() == NlsLoginError::InvalidProof);
}

TEST_CASE("LoginUserNls::verify maps NlsCryptoError::InvalidProof to "
          "NlsLoginError::InvalidProof",
          "[application][auth][nls]") {
    Fixture f;
    f.crypto.fail_with = NlsCryptoError::InvalidProof;
    auto uc = f.make();

    auto ch = uc.challenge("alice", any_A());
    REQUIRE(ch);

    auto A = any_A();
    auto M1 = any_M1();
    auto r = uc.verify("alice", ch.value().crypto_ctx, A, M1, f.acct);

    REQUIRE_FALSE(r);
    CHECK(r.error() == NlsLoginError::InvalidProof);
}

TEST_CASE("LoginUserNls::verify returns the server proof M2 and threads the "
          "account_id on success",
          "[application][auth][nls]") {
    Fixture f;
    f.crypto.fail_with = std::nullopt;       // succeed
    f.crypto.m2.fill(std::byte{0x5A});
    auto uc = f.make();

    auto ch = uc.challenge("alice", any_A());
    REQUIRE(ch);
    // challenge() must have carried the stored account_id through.
    CHECK(ch.value().account_id == f.acct);

    auto A = any_A();
    auto M1 = any_M1();
    auto r = uc.verify("alice", ch.value().crypto_ctx, A, M1,
                       ch.value().account_id);

    REQUIRE(r);
    CHECK(r.value().account_id == f.acct);
    CHECK(r.value().server_proof == f.crypto.m2);
}
