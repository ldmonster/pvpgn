// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file nls_crypto.hpp
/// Application-layer port for NLS (SRP-6a) crypto.
///
/// Defined here so that `application/auth` and `application/connection` can
/// drive the NLS challenge/proof round-trip without depending on
/// `infra/crypto/nls.hpp` (which would violate the hexagonal layering rule
/// `application MUST NOT depend on infra`).
///
/// The concrete implementation lives in `infra/crypto/nls_crypto_adapter.hpp`
/// and is wired into use-cases by the composition root.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/result.hpp"

namespace pvpgn::application::auth {

/// Error vocabulary for the crypto port (mirrors infra::crypto::NlsError).
enum class NlsCryptoError : std::uint8_t {
    InvalidProof,      ///< Client M1 proof did not match
    InvalidPublicKey,  ///< Client A == 0 mod N (protocol violation)
    CryptoError,       ///< Internal OpenSSL / allocation failure
};

/// Opaque per-session SRP-6a server state.
///
/// Field layout mirrors `infra::crypto::NlsContext` exactly so the adapter
/// can copy field-by-field without conversion. The struct intentionally
/// lives in the application layer (not domain) because NLS is an
/// authentication-protocol concern, not a core domain concept.
struct NlsCryptoContext {
    /// 32-byte random salt sent to the client (s).
    std::array<std::byte, 32>  salt{};
    /// 32-byte random server private key (b).
    std::array<std::byte, 32>  server_private_key{};
    /// 128-byte server public key sent to the client (B).
    std::array<std::byte, 128> server_public_key{};
    /// 40-byte interleaved session key (K), populated after verify_proof().
    std::array<std::byte, 40>  session_key{};
};

/// Port: stateless NLS server-side crypto operations.
///
/// Implementations live in the infra layer. The application layer holds
/// only a non-owning reference to the port.
class INlsCryptoService {
public:
    virtual ~INlsCryptoService() = default;

    /// Generate a server challenge for the given account.
    ///
    /// @param username     Account name (ASCII, case-insensitive).
    /// @param verifier     128-byte stored verifier (v = g^x mod N).
    /// @param stored_salt  32-byte salt stored with the account.
    /// @returns            Populated NlsCryptoContext.
    [[nodiscard]] virtual NlsCryptoContext create_challenge(
        std::string_view           username,
        std::span<const std::byte> verifier,
        std::span<const std::byte> stored_salt) = 0;

    /// Verify the client proof M1 and compute the server proof M2.
    ///
    /// @param ctx                 Context returned by create_challenge();
    ///                            updated in place with the session key.
    /// @param username            Account name (must match challenge).
    /// @param client_public_key_A Client public key A.
    /// @param client_proof_M1     20-byte client proof M1.
    /// @returns                   20-byte server proof M2 on success.
    [[nodiscard]] virtual core::Result<std::array<std::byte, 20>, NlsCryptoError>
    verify_proof(NlsCryptoContext&          ctx,
                 std::string_view           username,
                 std::span<const std::byte> client_public_key_A,
                 std::span<const std::byte> client_proof_M1) = 0;
};

}  // namespace pvpgn::application::auth
