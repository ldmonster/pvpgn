// SPDX-License-Identifier: GPL-2.0-or-later
//
// Catch2 port of the legacy `src/test/bnetsrp3_test.cpp` standalone
// assertion-based test, brought under the v3 test tree as part of
// `plans/refactoring-plan-testing.md` "Legacy Test Removal".
//
// Exercises the legacy `pvpgn::BnetSRP3` BNCS challenge-response
// helper (the algorithm used by the SID_AUTH_CHECK login flow):
// derives client + server session keys from the shared password and
// asserts both sides agree on the hashed secret.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "common/setup_before.h"
#include "common/bigint.h"
#include "common/bnetsrp3.h"
#include "common/setup_after.h"

using pvpgn::BigInt;
using pvpgn::BnetSRP3;

TEST_CASE("BnetSRP3: client and server hashed secrets agree",
          "[common][bnetsrp3]") {
    // Fixed salt — keep the test deterministic.
    const unsigned char salt_bytes[] = {
        0xB3, 0x46, 0x25, 0x10, 0x1D, 0xEA, 0x80, 0xB9,
        0x92, 0xEB, 0x50, 0x4E, 0x84, 0x00, 0x06, 0xA1,
        0x7E, 0x77, 0x58, 0x66, 0x73, 0x89, 0x27, 0xF7,
        0x14, 0x90, 0x2D, 0xA6, 0x3F, 0xCC, 0xF8, 0x52
    };

    BnetSRP3 client("regen", "bogen");
    client.setSalt(BigInt(salt_bytes, 32));

    BigInt verifier = client.getVerifier();
    BigInt salt     = client.getSalt();

    BnetSRP3 server("power", salt);

    BigInt clientPub = client.getClientSessionPublicKey();
    BigInt serverPub = server.getServerSessionPublicKey(verifier);

    const BigInt clientSecret = client.getHashedClientSecret(serverPub);
    const BigInt serverSecret = server.getHashedServerSecret(clientPub,
                                                             verifier);

    CHECK(clientSecret == serverSecret);
}
