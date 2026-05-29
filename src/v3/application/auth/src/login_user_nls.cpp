// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/login_user_nls.hpp"

#include <span>

#include "infra/crypto/nls.hpp"

namespace pvpgn::application::auth {

// ---------------------------------------------------------------------------
// Step 1: challenge
// ---------------------------------------------------------------------------

core::Result<NlsChallengeResult, NlsLoginError>
LoginUserNls::challenge(std::string_view username,
                        core::ByteView   /*client_public_key_A*/)
{
    // 1. Look up stored NLS credentials (verifier + salt) for this account.
    auto creds = credentials_.find(username);
    if (!creds) {
        return core::fail(NlsLoginError::AccountNotFound);
    }

    // 2. Generate the server challenge.
    //    NlsServer::create_challenge() is noexcept-ish but may throw on
    //    catastrophic OpenSSL failure; we let that propagate as a hard error.
    infra::crypto::NlsContext ctx =
        infra::crypto::NlsServer::create_challenge(
            username,
            std::span<const std::byte>{creds->verifier},
            std::span<const std::byte>{creds->salt});

    // 3. Build the result.  The salt in the context is the one we just
    //    passed in (create_challenge copies it into ctx.salt).
    NlsChallengeResult result;
    result.salt              = ctx.salt;
    result.server_public_key = ctx.server_public_key;
    result.crypto_ctx        = std::move(ctx);
    // Stash the account ID so verify() can thread it into NlsProofResult.
    result.account_id        = creds->account_id;

    return result;
}

// ---------------------------------------------------------------------------
// Step 2: verify
// ---------------------------------------------------------------------------

core::Result<NlsProofResult, NlsLoginError>
LoginUserNls::verify(std::string_view                  username,
                     const infra::crypto::NlsContext&  ctx,
                     core::ByteView                    client_public_key_A,
                     core::ByteView                    client_proof_M1,
                     domain::AccountId                 account_id)
{
    // NlsServer::verify_proof() takes a mutable context (it writes the
    // session key K into it).  We work on a local copy so the caller's
    // const reference is not violated.
    infra::crypto::NlsContext mutable_ctx = ctx;

    auto proof_result = infra::crypto::NlsServer::verify_proof(
        mutable_ctx,
        username,
        client_public_key_A,
        client_proof_M1);

    if (!proof_result) {
        const auto nls_err = proof_result.error();
        switch (nls_err) {
            case infra::crypto::NlsError::InvalidProof:
                return core::fail(NlsLoginError::InvalidProof);
            case infra::crypto::NlsError::InvalidPublicKey:
                // Treat a zero/invalid A as a proof failure (protocol violation).
                return core::fail(NlsLoginError::InvalidProof);
            case infra::crypto::NlsError::CryptoError:
                return core::fail(NlsLoginError::CryptoError);
        }
        // Unreachable, but keeps compilers happy.
        return core::fail(NlsLoginError::CryptoError);
    }

    NlsProofResult result;
    result.server_proof = proof_result.value();
    result.account_id   = account_id;
    return result;
}

}  // namespace pvpgn::application::auth
