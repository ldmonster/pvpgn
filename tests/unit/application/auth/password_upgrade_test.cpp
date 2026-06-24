// SPDX-License-Identifier: GPL-2.0-or-later
//
// Unit tests for the pure PasswordUpgrade policy, using a deterministic
// StubPasswordHasher (no libsodium needed). The stub models a two-version
// scheme so the transparent-upgrade path is exercised end to end:
//   - "v2:" prefix  = current algorithm (no rehash needed)
//   - "v1:" prefix  = legacy algorithm (verifies, but needs rehash)
//   - anything else = unrecognised  (treated as needs-rehash on the
//                     argon2id semantics where -1 => upgrade)

#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "application/auth/password_upgrade.hpp"
#include "core/crypto/password_hasher.hpp"

using pvpgn::application::auth::PasswordUpgrade;
using pvpgn::core::crypto::IPasswordHasher;
using pvpgn::core::crypto::PasswordHashAlgorithm;

namespace {

/// Deterministic, INSECURE, test-only hasher. Encodes `"<ver>:<plaintext>"`
/// so verification is an exact suffix compare and upgrades are observable.
/// Never use outside tests — it stores the plaintext verbatim.
class StubPasswordHasher final : public IPasswordHasher {
public:
    static constexpr std::string_view kCurrent = "v2:";
    static constexpr std::string_view kLegacy  = "v1:";

    std::string hash(std::string_view password) override {
        return std::string(kCurrent) + std::string(password);
    }

    bool verify(std::string_view password,
                std::string_view encoded) const override {
        const auto body = strip_version(encoded);
        return body.has_value() && *body == password;
    }

    bool needs_rehash(std::string_view encoded) const override {
        // Current-version hashes are fine; legacy or unrecognised need rehash.
        return !encoded.starts_with(kCurrent);
    }

    PasswordHashAlgorithm algorithm() const noexcept override {
        return PasswordHashAlgorithm::argon2id;
    }

private:
    // Returns the plaintext body for a recognised "v1:"/"v2:" encoding.
    static std::optional<std::string_view> strip_version(std::string_view e) {
        if (e.starts_with(kCurrent)) return e.substr(kCurrent.size());
        if (e.starts_with(kLegacy))  return e.substr(kLegacy.size());
        return std::nullopt;  // unrecognised → cannot verify
    }
};

}  // namespace

TEST_CASE("PasswordUpgrade: current-version hash verifies without upgrade",
          "[application][auth][password_upgrade]") {
    StubPasswordHasher hasher;
    PasswordUpgrade policy{hasher};

    const std::string stored = hasher.hash("hunter2");  // "v2:hunter2"
    auto const r = policy.verify(stored, "hunter2");

    CHECK(r.verified);
    CHECK_FALSE(r.upgraded());
    CHECK_FALSE(r.upgraded_hash.has_value());
}

TEST_CASE("PasswordUpgrade: legacy hash verifies AND upgrades to current",
          "[application][auth][password_upgrade]") {
    StubPasswordHasher hasher;
    PasswordUpgrade policy{hasher};

    const std::string legacy = "v1:hunter2";  // legacy-version stored hash
    auto const r = policy.verify(legacy, "hunter2");

    REQUIRE(r.verified);
    REQUIRE(r.upgraded());
    // The upgraded hash is the current-version encoding of the same plaintext.
    CHECK(*r.upgraded_hash == "v2:hunter2");
    // And the upgraded hash itself no longer needs a rehash.
    CHECK_FALSE(hasher.needs_rehash(*r.upgraded_hash));
}

TEST_CASE("PasswordUpgrade: wrong password neither verifies nor upgrades",
          "[application][auth][password_upgrade]") {
    StubPasswordHasher hasher;
    PasswordUpgrade policy{hasher};

    SECTION("against a current-version hash") {
        auto const r = policy.verify("v2:hunter2", "wrong");
        CHECK_FALSE(r.verified);
        CHECK_FALSE(r.upgraded());
    }
    SECTION("against a legacy hash (must not leak an upgrade on failure)") {
        auto const r = policy.verify("v1:hunter2", "wrong");
        CHECK_FALSE(r.verified);
        CHECK_FALSE(r.upgraded());
        CHECK_FALSE(r.upgraded_hash.has_value());
    }
}

TEST_CASE("PasswordUpgrade: unrecognised stored hash fails closed",
          "[application][auth][password_upgrade]") {
    StubPasswordHasher hasher;
    PasswordUpgrade policy{hasher};

    // A stored value the hasher cannot parse must NOT verify, even with the
    // matching plaintext — and therefore must not upgrade.
    auto const r = policy.verify("garbage-hash", "hunter2");
    CHECK_FALSE(r.verified);
    CHECK_FALSE(r.upgraded());
}

TEST_CASE("PasswordUpgrade: round-trips a freshly-hashed password",
          "[application][auth][password_upgrade]") {
    StubPasswordHasher hasher;
    PasswordUpgrade policy{hasher};

    const std::string fresh = hasher.hash("correct horse battery staple");
    auto const r = policy.verify(fresh, "correct horse battery staple");

    CHECK(r.verified);
    CHECK_FALSE(r.upgraded());
}
