// SPDX-License-Identifier: GPL-2.0-or-later
//
// infra/crypto/nls_verifier.hpp -- NLS/SRP-6a verifier and password
// hash helpers for Warcraft III / W3XP account creation.
//
// Used during account registration to derive the stored (salt, verifier)
// pair from a plaintext password, and during login to re-derive the
// verifier from stored credentials.

#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace pvpgn::infra::crypto {

/// Stateless NLS verifier helper.
class NlsVerifier {
public:
    NlsVerifier() = delete;

    /// Create a (salt, verifier) pair for account registration.
    ///
    /// Generates a fresh random 32-byte salt, then computes:
    ///   x = H(s || H(uppercase(username) ":" uppercase(password)))
    ///   v = g^x mod N
    ///
    /// @param username  Account name (ASCII; uppercased internally).
    /// @param password  Plaintext password (ASCII; uppercased internally).
    /// @returns         {salt (32 bytes), verifier (128 bytes)}.
    [[nodiscard]] static std::pair<std::array<std::byte, 32>,
                                   std::array<std::byte, 128>>
    create_verifier(std::string_view username, std::string_view password);

    /// Compute the NLS password hash x = H(s || H(U:P)).
    ///
    /// This is the private key `x` used in SRP-6a.  Exposed for
    /// testing and for callers that need to re-derive x from stored
    /// credentials without going through the full verifier path.
    ///
    /// @param username  Account name (ASCII; uppercased internally).
    /// @param password  Plaintext password (ASCII; uppercased internally).
    /// @param salt      32-byte salt stored with the account.
    /// @returns         20-byte SHA-1 digest (x).
    [[nodiscard]] static std::array<std::byte, 20>
    hash_password(std::string_view           username,
                  std::string_view           password,
                  std::span<const std::byte> salt);
};

}  // namespace pvpgn::infra::crypto
