// SPDX-License-Identifier: GPL-2.0-or-later
//
// infra/crypto/nls.hpp -- NLS/SRP-6a server-side authentication for
// Warcraft III / W3XP (Battle.net "New Login System").
//
// Parameters:
//   N  = 1024-bit (128-byte) prime (WAR3 NLS prime)
//   g  = 47 (0x2F)
//   H  = SHA-1 (20 bytes)
//   K  = 40-byte interleaved session key
//
// Server flow:
//   1. create_challenge()  -- generates B (server public key) and salt
//   2. verify_proof()      -- verifies M1, returns M2 on success
//
// References:
//   http://www.javaop.com/@ron/documents/SRP.html
//   RFC 2945 (SRP-3), SRP-6a extensions

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "core/result.hpp"

namespace pvpgn::infra::crypto {

// ---- error type -----------------------------------------------------------

enum class NlsError : std::uint8_t {
    InvalidProof,      ///< Client M1 proof did not match
    InvalidPublicKey,  ///< Client A == 0 mod N (protocol violation)
    CryptoError,       ///< Internal OpenSSL / allocation failure
};

// ---- per-session state ----------------------------------------------------

/// Holds all server-side state for one NLS authentication session.
/// Created by NlsServer::create_challenge() and passed unchanged to
/// NlsServer::verify_proof().
struct NlsContext {
    /// 32-byte random salt sent to the client (s).
    std::array<std::byte, 32> salt{};

    /// 32-byte random server private key (b).
    std::array<std::byte, 32> server_private_key{};

    /// 128-byte server public key sent to the client (B).
    std::array<std::byte, 128> server_public_key{};

    /// 40-byte interleaved session key (K), populated after verify_proof().
    std::array<std::byte, 40> session_key{};
};

// ---- server API -----------------------------------------------------------

/// Stateless NLS server helper.  All methods are static; per-session
/// state is carried in NlsContext.
class NlsServer {
public:
    NlsServer() = delete;

    /// Generate a server challenge for the given account.
    ///
    /// @param username     Account name (ASCII, case-insensitive).
    /// @param verifier     128-byte stored verifier (v = g^x mod N).
    /// @param stored_salt  32-byte salt stored with the account.
    /// @returns            Populated NlsContext with salt, server_private_key,
    ///                     and server_public_key filled in.
    [[nodiscard]] static NlsContext
    create_challenge(std::string_view          username,
                     std::span<const std::byte> verifier,
                     std::span<const std::byte> stored_salt);

    /// Verify the client proof M1 and compute the server proof M2.
    ///
    /// @param ctx               Context returned by create_challenge().
    /// @param username          Account name (must match create_challenge).
    /// @param client_public_key_A  32-byte client public key A.
    /// @param client_proof_M1   20-byte client proof M1.
    /// @returns                 20-byte server proof M2 on success, or
    ///                          NlsError on failure.
    [[nodiscard]] static pvpgn::core::Result<std::array<std::byte, 20>, NlsError>
    verify_proof(NlsContext&                ctx,
                 std::string_view           username,
                 std::span<const std::byte> client_public_key_A,
                 std::span<const std::byte> client_proof_M1);
};

}  // namespace pvpgn::infra::crypto
