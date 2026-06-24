// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/core/secure_random_test.cpp
//
// Unit tests for core::crypto::SecureRandom. We cannot assert specific values
// from a CSPRNG, so these tests pin its *contract*: it fills the requested
// span exactly, honours inclusive uniform() bounds, handles edge cases
// (empty span, min==max, full uint32 range), and produces non-degenerate
// output (overwhelmingly likely to differ across draws).

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include "core/crypto/secure_random.hpp"

using pvpgn::core::crypto::SecureRandom;
using pvpgn::core::crypto::secure_random_bytes;

TEST_CASE("SecureRandom::fill_bytes fills the whole span", "[core][crypto][random]") {
    SecureRandom rng;
    std::array<std::byte, 64> buf;
    buf.fill(std::byte{0xAB});  // sentinel

    rng.fill_bytes(buf);

    // Overwhelmingly unlikely that all 64 bytes remain the 0xAB sentinel.
    bool all_sentinel = true;
    for (auto b : buf) {
        if (b != std::byte{0xAB}) { all_sentinel = false; break; }
    }
    CHECK_FALSE(all_sentinel);
}

TEST_CASE("SecureRandom::fill_bytes on empty span is a no-op", "[core][crypto][random]") {
    SecureRandom rng;
    CHECK_NOTHROW(rng.fill_bytes(std::span<std::byte>{}));
}

TEST_CASE("SecureRandom::uniform with min==max returns that value",
          "[core][crypto][random]") {
    SecureRandom rng;
    for (std::uint32_t v : {0u, 1u, 42u, 0xFFFFFFFFu}) {
        CHECK(rng.uniform(v, v) == v);
    }
}

TEST_CASE("SecureRandom::uniform stays within the inclusive range",
          "[core][crypto][random]") {
    SecureRandom rng;
    constexpr std::uint32_t lo = 10, hi = 20;
    bool saw_lo = false, saw_hi = false;
    for (int i = 0; i < 5000; ++i) {
        std::uint32_t v = rng.uniform(lo, hi);
        REQUIRE(v >= lo);
        REQUIRE(v <= hi);
        if (v == lo) saw_lo = true;
        if (v == hi) saw_hi = true;
    }
    // Both endpoints are reachable (inclusive bounds). Over 5000 draws across
    // 11 values the chance of missing an endpoint is negligible.
    CHECK(saw_lo);
    CHECK(saw_hi);
}

TEST_CASE("SecureRandom::uniform spans the full uint32 range without UB",
          "[core][crypto][random]") {
    SecureRandom rng;
    // range == 2^32 exercises the limit computation's wraparound guard.
    CHECK_NOTHROW(rng.uniform(0u, 0xFFFFFFFFu));
}

TEST_CASE("SecureRandom produces non-degenerate output", "[core][crypto][random]") {
    SecureRandom rng;
    std::set<std::uint64_t> seen;
    for (int i = 0; i < 256; ++i) seen.insert(rng.next_u64());
    // 256 draws from a 64-bit space: collisions are astronomically unlikely.
    CHECK(seen.size() == 256u);
}

TEST_CASE("secure_random_bytes free helper fills the span", "[core][crypto][random]") {
    std::array<std::byte, 32> a, b;
    a.fill(std::byte{0});
    b.fill(std::byte{0});
    secure_random_bytes(a);
    secure_random_bytes(b);
    // Two independent draws should differ.
    CHECK(a != b);
}
