// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/crypto/bnet_srp3_golden_test.cpp -- Plan 08.
//
// Golden-vector / round-trip regression tests for the Battle.net SRP-3
// implementation (`infra::crypto::BnetSrp3`). Two complementary guards:
//
//   1. ROUND-TRIP (self-validating): drive a full deterministic client/server
//      handshake and assert the SRP invariant — both sides derive the SAME
//      session key K, and each side's password proof verifies on the other.
//      This proves protocol correctness without any captured fixtures.
//
//   2. FROZEN WIRE VECTORS (characterization): pin the exact hex of the
//      verifier, public keys, session key and proofs for a fixed
//      (username, password, salt, a, b). Any change to the SRP math that alters
//      the bytes on the wire — i.e. would break compatibility with shipped
//      clients — fails this test. The values were captured from the current
//      implementation, which is parity-verified against the legacy
//      `pvpgn::BnetSRP3` (see parity_test.cpp). Replacing/augmenting them with
//      vectors captured from real client builds is tracked in Plan 08.

#include <array>
#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "infra/crypto/big_uint.hpp"
#include "infra/crypto/bnet_srp3.hpp"

using pvpgn::v3::infra::crypto::BigUInt;
using pvpgn::v3::infra::crypto::BnetSrp3;

namespace {

// Fixed, deterministic handshake inputs. Account name is upper-cased by the
// SRP-3 string handling; password is the secret. Salt and the per-session
// private keys (client `a`, server `b`) are pinned so every derived value is
// reproducible.
constexpr std::string_view kUser = "GOLDEN";
constexpr std::string_view kPass = "correcthorse";

// 32-byte salt.
constexpr std::string_view kSaltHex =
    "00112233445566778899aabbccddeeff"
    "0123456789abcdef0123456789abcdef";

// Client session private key `a` (256-bit).
constexpr std::string_view kClientPrivHex =
    "a1a2a3a4a5a6a7a8a9aaabacadaeafb0"
    "b1b2b3b4b5b6b7b8b9babbbcbdbebfc0";

// Server session private key `b` (256-bit).
constexpr std::string_view kServerPrivHex =
    "0102030405060708090a0b0c0d0e0f10"
    "1112131415161718191a1b1c1d1e1f20";

BigUInt salt()        { return BigUInt::from_hex(kSaltHex); }
BigUInt client_priv() { return BigUInt::from_hex(kClientPrivHex); }
BigUInt server_priv() { return BigUInt::from_hex(kServerPrivHex); }

// Build a deterministic client side (knows the password).
BnetSrp3 make_client() {
    BnetSrp3 c{kUser, kPass};
    c.set_salt(salt());
    c.set_client_private_key(client_priv());
    return c;
}

// Build a deterministic server side (knows only username + salt).
BnetSrp3 make_server() {
    BnetSrp3 s{kUser, salt()};
    s.set_server_private_key(server_priv());
    return s;
}

}  // namespace

TEST_CASE("BnetSrp3 golden: full handshake derives a shared session key",
          "[infra][crypto][srp][golden]") {
    BnetSrp3 client = make_client();
    BnetSrp3 server = make_server();

    // Registration artefact: the verifier the server stores for this account.
    const BigUInt v = client.verifier();

    // Public ephemeral keys exchanged on the wire.
    const BigUInt A = client.client_session_public_key();
    const BigUInt B = server.server_session_public_key(v);

    // Each side independently derives the shared secret hash K.
    const BigUInt K_client = client.hashed_client_secret(B);
    const BigUInt K_server = server.hashed_server_secret(A, v);

    // THE SRP invariant: both parties arrive at the same session key.
    CHECK(K_client == K_server);

    // Client proves knowledge of the password; the proof is reproducible and
    // the server can recompute the same expected value.
    const BigUInt M1 = client.client_password_proof(A, B, K_client);

    // Server acknowledges with its own proof; the client recomputes it and the
    // two must match (mutual authentication).
    const BigUInt M2_server = server.server_password_proof(A, M1, K_server);
    const BigUInt M2_client = client.server_password_proof(A, M1, K_client);
    CHECK(M2_server == M2_client);
}

TEST_CASE("BnetSrp3 golden: wire vectors are bit-stable",
          "[infra][crypto][srp][golden]") {
    BnetSrp3 client = make_client();
    BnetSrp3 server = make_server();

    const BigUInt v  = client.verifier();
    const BigUInt A  = client.client_session_public_key();
    const BigUInt B  = server.server_session_public_key(v);
    const BigUInt K  = client.hashed_client_secret(B);
    const BigUInt M1 = client.client_password_proof(A, B, K);
    const BigUInt M2 = server.server_password_proof(A, M1, K);

    // Frozen golden values (hex, as BigUInt::to_hex emits them — leading-zero
    // nibbles are not padded). Captured from the parity-verified implementation;
    // a mismatch means the on-wire SRP bytes changed.
    CHECK(v.to_hex()  ==
          "29af1b9c11c85cc6dce4b6e4b649c0b4af9edccb16b2ac6c8a06d94ad38dd806");
    CHECK(A.to_hex()  ==
          "ff153b11871a7d3aa77edd3f1575c2b4fbad0d550e6df0222ccb0b7eb93952b");
    CHECK(B.to_hex()  ==
          "4643f16f0cdd83558b6881281a67397db8f635c80160d7b4dca1d99592b6d3fc");
    CHECK(K.to_hex()  ==
          "82472ed3e115f06ba57a8ac31683a8464aa1a9c3902b3c003eff58608d9c9876"
          "524cf02f0b0e0d4b");
    CHECK(M1.to_hex() == "9454d79ec5c50593b27ad845847a0b9b4551b2fb");
    CHECK(M2.to_hex() == "ea80de605bc99538ae64f0ec72299acd6e71b53");
}
