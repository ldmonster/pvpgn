// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file login_user_w3.hpp
/// WarCraft III SRP-3 login use-case (SID_AUTH_ACCOUNTLOGON 0x53 / PROOF 0x54).
///
/// Mirrors the original server's `_client_loginreqw3`: the entire challenge is
/// computed up front in challenge() — the server public key B sent to the
/// client, the EXPECTED client proof M1 (compared byte-for-byte against the
/// client's 0x54 proof), and the server proof M2 (returned to the client on a
/// match). The FSM holds the (M1, M2, account_id) triple between the two steps;
/// verify() is then a constant-time-ish 20-byte comparison.
///
/// The crypto is the legacy-bit-compatible [[bnet_srp3]] (32-byte modulus), so
/// the bytes on the wire match what real WAR3/W3XP clients expect.

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"

#include "application/auth/srp3_credential_store.hpp"

namespace pvpgn::application::auth {

enum class W3LoginError {
    AccountNotFound,  ///< no account / no SRP-3 credentials for the username
    CryptoError,      ///< internal SRP failure
};

/// Everything the FSM needs after SID_AUTH_ACCOUNTLOGON (0x53): the salt + B to
/// send to the client, and the M1/M2/account_id to hold for the 0x54 proof.
struct W3ChallengeResult {
    std::array<std::uint8_t, 32> salt{};                 ///< echoed to client
    std::array<std::uint8_t, 32> server_public_key{};    ///< B, sent to client
    std::array<std::uint8_t, 20> expected_client_proof{};///< M1 to match
    std::array<std::uint8_t, 20> server_proof{};         ///< M2 to return
    domain::AccountId            account_id{0};
};

/// Stateless SRP-3 login use-case. Session attachment is the FSM's job.
class LoginUserW3 {
public:
    explicit LoginUserW3(const ISrp3CredentialStore& store) noexcept
        : store_(store) {}

    /// Step 1 (0x53): look up the account's SRP-3 credentials, derive B and the
    /// expected proofs from the client public key A. `client_public_key_A` is
    /// the 32 wire bytes from the LOGINREQ_W3 packet.
    [[nodiscard]] core::Result<W3ChallengeResult, W3LoginError>
    challenge(std::string_view username,
              std::span<const std::uint8_t, 32> client_public_key_A) const;

private:
    const ISrp3CredentialStore& store_;
};

}  // namespace pvpgn::application::auth
