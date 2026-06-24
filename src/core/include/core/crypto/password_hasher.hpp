// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file password_hasher.hpp
/// `core::crypto::IPasswordHasher` — the password-AT-REST hashing port.
///
/// Replaces the legacy password-equivalent storage (Blizzard SHA-1
/// `bnethash`) with a salted, memory-hard argon2id hash. This interface is
/// what the application layer (account creation, login, password change)
/// depends on; the concrete argon2id adapter lives in `infra/crypto/` and is
/// built where libsodium is available.
///
/// NOTE — do not confuse this with `domain::identity::IPasswordHasher`, which
/// is a *session-hash* port: it derives the per-connection bnet session hash
/// `bnet_hash(client_token || server_token || hash1)` for the wire handshake
/// and has nothing to do with how passwords are stored at rest.

#include <cstdint>
#include <string>
#include <string_view>

namespace pvpgn::core::crypto {

/// Identifies the algorithm that produced (or should produce) a stored hash.
enum class PasswordHashAlgorithm : std::uint8_t {
    /// Modern memory-hard hash (libsodium `crypto_pwhash`, ARGON2ID13).
    argon2id = 0,
    /// Legacy Blizzard SHA-1 derivative (`bnethash`). Read-only: accepted on
    /// login for transparent upgrade, never written for new accounts.
    legacy_bnet = 1,
};

/// Port for hashing and verifying passwords at rest.
///
/// The encoded form returned by `hash()` is self-describing (for argon2id it
/// is the PHC string `$argon2id$v=19$m=...,t=...,p=...$salt$digest`), so
/// `verify()` and `needs_rehash()` need no out-of-band parameters.
class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;

    /// Hash `password`, returning the self-describing encoded hash to persist.
    /// Generates a fresh random salt internally.
    [[nodiscard]] virtual std::string hash(std::string_view password) = 0;

    /// Verify `password` against a previously-stored `encoded` hash.
    /// Implementations MUST compare in constant time. Returns false on any
    /// malformed/unsupported `encoded` input rather than throwing.
    [[nodiscard]] virtual bool verify(std::string_view password,
                                      std::string_view encoded) const = 0;

    /// True if `encoded` was produced by a superseded algorithm or weaker
    /// parameters than the current policy — i.e. the caller should re-hash
    /// the (now-verified) plaintext and persist the result.
    [[nodiscard]] virtual bool needs_rehash(std::string_view encoded) const = 0;

    /// The algorithm this hasher writes for new hashes.
    [[nodiscard]] virtual PasswordHashAlgorithm algorithm() const noexcept = 0;
};

}  // namespace pvpgn::core::crypto
