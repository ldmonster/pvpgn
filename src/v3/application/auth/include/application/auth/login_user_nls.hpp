// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file login_user_nls.hpp
/// Two-step NLS (New Login System) authentication use-case for WAR3/W3XP.
///
/// The NLS protocol uses SRP-6a:
///   Step 1 — challenge():
///     Client sends username + client public key A (SID_AUTH_ACCOUNTLOGON 0x53).
///     Server looks up the stored (salt, verifier) for the account, generates
///     a server public key B, and returns (salt s, server public key B) plus
///     an opaque NlsContext that the FSM must hold between the two steps.
///
///   Step 2 — verify():
///     Client sends proof M1 (SID_AUTH_ACCOUNTLOGONPROOF 0x54).
///     Server verifies M1 against the NlsContext from step 1 and, on success,
///     returns the server proof M2 to send back to the client.
///
/// This use-case is stateless: all per-session state is carried in the
/// NlsContext value returned from challenge() and passed back to verify().
/// The FSM layer is responsible for storing the context between the two calls.
///
/// NLS credentials (verifier + salt) are fetched via the INlsCredentialStore
/// port, which is separate from the main IAccountRepository so that the
/// domain Account aggregate is not burdened with SRP-specific fields.

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "core/bytes.hpp"
#include "core/result.hpp"
#include "domain/shared/ids.hpp"
#include "infra/crypto/nls.hpp"

namespace pvpgn::application::auth {

// ---------------------------------------------------------------------------
// NLS credential store port
// ---------------------------------------------------------------------------

/// NLS (SRP-6a) credentials stored per account.
struct NlsCredentials {
    /// 32-byte random salt (s) generated at account creation.
    std::array<std::byte, 32>  salt;
    /// 128-byte SRP verifier v = g^x mod N.
    std::array<std::byte, 128> verifier;
    /// Numeric account ID (used to populate NlsProofResult::account_id).
    domain::AccountId account_id{0};
};

/// Port: provides NLS credential lookup by username.
///
/// Implementations may back this with the legacy attribute bag
/// (`BNET\\acct\\nls_salt` / `BNET\\acct\\nls_verifier`), a dedicated
/// SQL table, or an in-memory map for tests.
class INlsCredentialStore {
public:
    virtual ~INlsCredentialStore() = default;

    /// Look up NLS credentials for the given username (case-insensitive).
    /// Returns `std::nullopt` if the account does not exist or has no NLS
    /// credentials registered.
    [[nodiscard]] virtual std::optional<NlsCredentials>
    find(std::string_view username) = 0;
};

// ---------------------------------------------------------------------------
// Error vocabulary
// ---------------------------------------------------------------------------

enum class NlsLoginError {
    AccountNotFound,  ///< No account / NLS credentials for the given username
    CryptoError,      ///< Internal SRP / OpenSSL failure
    InvalidProof,     ///< Client M1 proof did not match
    AlreadyLoggedIn,  ///< Session slot already occupied (reserved for FSM use)
};

// ---------------------------------------------------------------------------
// Result types
// ---------------------------------------------------------------------------

/// Returned by challenge(): everything the FSM needs to reply to
/// SID_AUTH_ACCOUNTLOGON and to hold for the subsequent verify() call.
struct NlsChallengeResult {
    /// 32-byte random salt (s) — stored with the account, sent to client.
    std::array<std::byte, 32>  salt;
    /// 128-byte server public key (B) — sent to client.
    std::array<std::byte, 128> server_public_key;
    /// Opaque per-session SRP state.  Must be passed unchanged to verify().
    infra::crypto::NlsContext  crypto_ctx;
    /// Account ID of the looked-up account.  Carried through to verify() so
    /// the FSM can populate NlsProofResult::account_id without a second lookup.
    domain::AccountId account_id{0};
};

/// Returned by verify(): the server proof M2 to send back to the client,
/// plus the authenticated account's ID.
struct NlsProofResult {
    /// 20-byte server proof (M2).
    std::array<std::byte, 20> server_proof;
    /// Account ID of the authenticated user.
    domain::AccountId account_id{0};
};

// ---------------------------------------------------------------------------
// Use-case
// ---------------------------------------------------------------------------

/// Stateless NLS authentication use-case.
///
/// Constructor-injects the NLS credential store.  Session attachment is
/// intentionally left to the FSM layer so this use-case stays pure.
class LoginUserNls {
public:
    explicit LoginUserNls(INlsCredentialStore& credentials) noexcept
        : credentials_(credentials) {}

    /// Step 1: generate a server challenge for the given username.
    ///
    /// Looks up the NLS credentials (verifier + salt) for the account,
    /// calls NlsServer::create_challenge(), and returns the populated
    /// NlsChallengeResult.
    ///
    /// @param username             Account name (case-insensitive).
    /// @param client_public_key_A  128-byte client public key A (passed
    ///                             through for protocol completeness; the
    ///                             SRP challenge itself does not use A).
    /// @returns NlsChallengeResult on success, NlsLoginError on failure.
    [[nodiscard]] core::Result<NlsChallengeResult, NlsLoginError>
    challenge(std::string_view username,
              core::ByteView   client_public_key_A);

    /// Step 2: verify the client proof M1 and return the server proof M2.
    ///
    /// Delegates to NlsServer::verify_proof() using the NlsContext produced
    /// by challenge().  On success the account is considered authenticated;
    /// session attachment is the caller's responsibility.
    ///
    /// @param username             Account name (must match challenge()).
    /// @param ctx                  NlsContext returned by challenge().
    /// @param client_public_key_A  128-byte client public key A (same as
    ///                             supplied to challenge()).
    /// @param client_proof_M1      20-byte client proof M1.
    /// @param account_id           Account ID from the challenge step
    ///                             (NlsChallengeResult::account_id); threaded
    ///                             through into NlsProofResult::account_id.
    /// @returns NlsProofResult (M2 + account_id) on success, NlsLoginError on failure.
    [[nodiscard]] core::Result<NlsProofResult, NlsLoginError>
    verify(std::string_view                  username,
           const infra::crypto::NlsContext&  ctx,
           core::ByteView                    client_public_key_A,
           core::ByteView                    client_proof_M1,
           domain::AccountId                 account_id);

private:
    INlsCredentialStore& credentials_;
};

}  // namespace pvpgn::application::auth
