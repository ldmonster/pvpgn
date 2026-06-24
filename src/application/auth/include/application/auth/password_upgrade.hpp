// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file password_upgrade.hpp
/// `PasswordUpgrade` — pure policy for transparent password-at-rest upgrade
/// on a plaintext-bearing authentication.
///
/// ## Scope: plaintext-bearing logins only
///
/// The classic Battle.net OLS/NLS auth is *challenge-response*: the client
/// never sends the plaintext password, only `hash2 = bnet_hash(...||hash1)` (or
/// an SRP proof). The server must keep `hash1` / the SRP verifier to validate,
/// so those flows cannot adopt a salted argon2id-of-the-plaintext at rest.
///
/// argon2id-at-rest therefore applies to the flows where the server DOES see
/// the plaintext: telnet plaintext login, account creation / password-set that
/// carries the plaintext, or a future web/admin API. This policy is the pure,
/// I/O-free core those flows call: verify the plaintext against the stored
/// at-rest hash and, on success, decide whether to re-hash (legacy algorithm
/// or weaker parameters → upgrade). The caller persists `result.upgraded_hash`
/// when present.
///
/// Pure: no repository, no clock, no logging. All collaborators injected.

#include <optional>
#include <string>
#include <string_view>

#include "core/crypto/password_hasher.hpp"

namespace pvpgn::application::auth {

/// Outcome of `PasswordUpgrade::verify`.
struct PasswordVerification {
    /// True when the plaintext matched the stored hash.
    bool verified = false;

    /// Present only when `verified` and the stored hash should be replaced
    /// (legacy algorithm or weaker-than-policy parameters). The caller
    /// persists this new encoded hash. Absent when no upgrade is warranted.
    std::optional<std::string> upgraded_hash;

    /// Convenience: did this verification produce an upgrade to persist?
    [[nodiscard]] bool upgraded() const noexcept {
        return upgraded_hash.has_value();
    }
};

/// Verifies a plaintext password against a stored at-rest hash and performs a
/// transparent rehash-on-success when the stored hash is stale.
class PasswordUpgrade {
public:
    explicit PasswordUpgrade(core::crypto::IPasswordHasher& hasher) noexcept
        : hasher_(hasher) {}

    /// Verify `plaintext` against `stored_encoded`. On a match, if the stored
    /// hash needs rehashing, also compute the replacement.
    ///
    /// On a non-match, returns `{verified=false}` with no upgrade — the caller
    /// must not persist anything.
    [[nodiscard]] PasswordVerification
    verify(std::string_view stored_encoded, std::string_view plaintext) const;

private:
    core::crypto::IPasswordHasher& hasher_;
};

}  // namespace pvpgn::application::auth
