// SPDX-License-Identifier: GPL-2.0-or-later
//
// Property test for `infra/legacy_crypto/BnetSessionHasher`: the
// adapter must produce byte-for-byte the same hash that the legacy
// `_client_changepassreq` handler computes when it concatenates
// (ticks || sessionkey || hash1) and feeds it to `pvpgn::bnet_hash`.
//
// Rather than embed magic numbers, we recompute the legacy answer
// in the test by calling `bnet_hash` directly on the same packed
// transcript -- this catches drift if the adapter ever changes its
// endianness or struct layout.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>

#include "infra/legacy_crypto/bnet_session_hasher.hpp"

#include "common/setup_before.h"
#include "common/bnethash.h"
#include "common/setup_after.h"

using pvpgn::infra::legacy_crypto::BnetSessionHasher;
using pvpgn::domain::BNHash;

namespace {

#pragma pack(push, 1)
struct Transcript {
    std::uint32_t ticks;
    std::uint32_t sessionkey;
    std::uint8_t  hash1[20];
};
#pragma pack(pop)

BNHash legacy_compute(std::uint32_t ticks,
                      std::uint32_t sessionkey,
                      const BNHash& hash1) {
    Transcript t{};
    t.ticks      = ticks;
    t.sessionkey = sessionkey;
    std::memcpy(t.hash1, hash1.bytes().data(), 20);

    ::pvpgn::t_hash out{};
    ::pvpgn::bnet_hash(&out, sizeof(t), &t);

    BNHash::Bytes bytes{};
    for (std::size_t i = 0; i < 5; ++i) {
        std::uint32_t w = out[i];
        bytes[i * 4 + 0] = static_cast<std::uint8_t>(w & 0xFF);
        bytes[i * 4 + 1] = static_cast<std::uint8_t>((w >> 8) & 0xFF);
        bytes[i * 4 + 2] = static_cast<std::uint8_t>((w >> 16) & 0xFF);
        bytes[i * 4 + 3] = static_cast<std::uint8_t>((w >> 24) & 0xFF);
    }
    return BNHash{bytes};
}

}  // namespace

TEST_CASE("BnetSessionHasher matches legacy bnet_hash on the all-zero transcript",
          "[infra][legacy_crypto][hasher]") {
    BnetSessionHasher hasher;
    BNHash::Bytes zero{};
    BNHash h1{zero};
    auto v3     = hasher.derive_session_hash(h1, 0u, 0u);
    auto legacy = legacy_compute(0u, 0u, h1);
    REQUIRE(v3 == legacy);
}

TEST_CASE("BnetSessionHasher matches legacy on a non-trivial fixture",
          "[infra][legacy_crypto][hasher]") {
    BnetSessionHasher hasher;
    BNHash::Bytes b{};
    for (std::size_t i = 0; i < 20; ++i) {
        b[i] = static_cast<std::uint8_t>(0xa5 ^ i);
    }
    BNHash h1{b};
    auto v3     = hasher.derive_session_hash(h1, 0xdeadbeefu, 0x12345678u);
    auto legacy = legacy_compute(0xdeadbeefu, 0x12345678u, h1);
    REQUIRE(v3 == legacy);
}

TEST_CASE("BnetSessionHasher is sensitive to ticks",
          "[infra][legacy_crypto][hasher]") {
    BnetSessionHasher hasher;
    BNHash::Bytes z{};
    BNHash h1{z};
    auto a = hasher.derive_session_hash(h1, 1u, 0u);
    auto b = hasher.derive_session_hash(h1, 2u, 0u);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("BnetSessionHasher is sensitive to sessionkey",
          "[infra][legacy_crypto][hasher]") {
    BnetSessionHasher hasher;
    BNHash::Bytes z{};
    BNHash h1{z};
    auto a = hasher.derive_session_hash(h1, 0u, 1u);
    auto b = hasher.derive_session_hash(h1, 0u, 2u);
    REQUIRE_FALSE(a == b);
}
