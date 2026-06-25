// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/auth/login_user_w3.hpp"

#include "infra/crypto/big_uint.hpp"
#include "infra/crypto/bnet_srp3.hpp"

namespace pvpgn::application::auth {

namespace {
using pvpgn::v3::infra::crypto::BigUInt;
using pvpgn::v3::infra::crypto::BnetSrp3;

// Wire conventions copied verbatim from the original server's
// _client_loginreqw3 (handle_bnet.cpp) so the bytes match real clients:
//   salt      : BigInt(buf, 32, 4, false)   -> block size 4
//   verifier  : BigInt(buf, 32, 1, false)   -> block size 1
//   A (client): BigInt(buf, 32, 1, false)   -> block size 1
//   B (server): getData(buf, 32, 4, false)  -> block size 4
//   proofs    : getData(buf, 20, 4, false)  -> block size 4
constexpr int kBlkSalt = 4;
constexpr int kBlkVerifier = 1;
constexpr int kBlkPublicKeyA = 1;
constexpr int kBlkPublicKeyB = 4;
constexpr int kBlkProof = 4;
}  // namespace

core::Result<W3ChallengeResult, W3LoginError>
LoginUserW3::challenge(std::string_view username,
                       std::span<const std::uint8_t, 32> client_public_key_A) const {
    const auto creds = store_.find(username);
    if (!creds) {
        return core::fail(W3LoginError::AccountNotFound);
    }

    const BigUInt salt =
        BigUInt::from_bytes_legacy(creds->salt, kBlkSalt, /*big_endian=*/false);
    const BigUInt verifier = BigUInt::from_bytes_legacy(
        creds->verifier, kBlkVerifier, /*big_endian=*/false);
    const BigUInt A = BigUInt::from_bytes_legacy(
        client_public_key_A, kBlkPublicKeyA, /*big_endian=*/false);

    // Server side knows username + salt; verifier is supplied per call.
    BnetSrp3 srp{username, salt};
    const BigUInt B = srp.server_session_public_key(verifier);
    const BigUInt K = srp.hashed_server_secret(A, verifier);
    const BigUInt M1 = srp.client_password_proof(A, B, K);
    const BigUInt M2 = srp.server_password_proof(A, M1, K);

    W3ChallengeResult out;
    out.salt = creds->salt;  // echoed verbatim
    B.to_bytes_legacy(out.server_public_key, kBlkPublicKeyB, /*big_endian=*/false);
    M1.to_bytes_legacy(out.expected_client_proof, kBlkProof, /*big_endian=*/false);
    M2.to_bytes_legacy(out.server_proof, kBlkProof, /*big_endian=*/false);
    out.account_id = creds->account_id;
    return out;
}

}  // namespace pvpgn::application::auth
