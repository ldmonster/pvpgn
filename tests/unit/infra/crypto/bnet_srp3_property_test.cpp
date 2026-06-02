// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/crypto/bnet_srp3_property_test.cpp -- Plan 10 property tests.
//
// Mathematical-invariant properties for the Battle.net SRP-3 handshake
// (`infra::crypto::BnetSrp3`). For randomized credentials and randomized
// per-session private keys/salt, the protocol must ALWAYS converge:
//   * both sides derive the same session key K, and
//   * the client and server password proofs agree (mutual authentication).
//
// A fixed seed makes any failure reproducible. No rapidcheck dependency.

#include <array>
#include <cstdint>
#include <random>
#include <span>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "infra/crypto/big_uint.hpp"
#include "infra/crypto/bnet_srp3.hpp"

using pvpgn::v3::infra::crypto::BigUInt;
using pvpgn::v3::infra::crypto::BnetSrp3;

namespace {

struct Gen {
    std::mt19937 rng{0x5A1700C0u};

    // A non-zero 256-bit value (private keys / salt). Forcing the low bit set
    // avoids the degenerate zero exponent.
    BigUInt big() {
        std::array<std::uint8_t, 32> b{};
        for (auto& x : b) x = static_cast<std::uint8_t>(rng());
        b[0] |= 0x01u;
        return BigUInt::from_bytes(std::span<const std::uint8_t>{b.data(), b.size()});
    }

    // A NUL-free account/password token of 1..16 chars.
    std::string token() {
        const std::size_t n = 1 + (rng() % 16);
        std::string s;
        s.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            s.push_back(static_cast<char>(0x41 + (rng() % 26)));  // A-Z
        }
        return s;
    }
};

}  // namespace

TEST_CASE("BnetSrp3 property: handshake always derives a shared session key",
          "[infra][crypto][srp][property]") {
    Gen g;
    for (int i = 0; i < 200; ++i) {
        const std::string user = g.token();
        const std::string pass = g.token();
        const BigUInt salt = g.big();
        const BigUInt a    = g.big();  // client session private key
        const BigUInt b    = g.big();  // server session private key

        INFO("iteration " << i << " user=" << user);

        BnetSrp3 client{user, pass};
        client.set_salt(salt);
        client.set_client_private_key(a);

        BnetSrp3 server{user, salt};
        server.set_server_private_key(b);

        const BigUInt v = client.verifier();
        const BigUInt A = client.client_session_public_key();
        const BigUInt B = server.server_session_public_key(v);

        const BigUInt k_client = client.hashed_client_secret(B);
        const BigUInt k_server = server.hashed_server_secret(A, v);

        // Invariant 1: both parties derive the same session key.
        REQUIRE(k_client == k_server);

        // Invariant 2: the proofs agree (mutual authentication).
        const BigUInt m1 = client.client_password_proof(A, B, k_client);
        const BigUInt m2_server = server.server_password_proof(A, m1, k_server);
        const BigUInt m2_client = client.server_password_proof(A, m1, k_client);
        REQUIRE(m2_server == m2_client);
    }
}

TEST_CASE("BnetSrp3 property: a wrong password breaks the shared key",
          "[infra][crypto][srp][property]") {
    Gen g;
    int diverged = 0;
    for (int i = 0; i < 100; ++i) {
        const std::string user = g.token();
        const std::string pass = g.token();
        std::string wrong = pass + "X";  // guaranteed different

        const BigUInt salt = g.big();
        const BigUInt a    = g.big();
        const BigUInt b    = g.big();

        INFO("iteration " << i << " user=" << user);

        // Server stores the verifier for the REAL password.
        BnetSrp3 real_client{user, pass};
        real_client.set_salt(salt);
        const BigUInt v = real_client.verifier();

        // A client that knows the WRONG password attempts the handshake.
        BnetSrp3 bad_client{user, wrong};
        bad_client.set_salt(salt);
        bad_client.set_client_private_key(a);

        BnetSrp3 server{user, salt};
        server.set_server_private_key(b);

        const BigUInt A = bad_client.client_session_public_key();
        const BigUInt B = server.server_session_public_key(v);

        const BigUInt k_client = bad_client.hashed_client_secret(B);
        const BigUInt k_server = server.hashed_server_secret(A, v);

        // With the wrong password the session keys must NOT match (overwhelming
        // probability; count to assert it actually happens every time).
        if (k_client != k_server) ++diverged;
    }
    CHECK(diverged == 100);
}
