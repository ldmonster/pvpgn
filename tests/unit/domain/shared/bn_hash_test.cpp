// SPDX-License-Identifier: GPL-2.0-or-later

/// @file bn_hash_test.cpp
/// Unit tests for domain::BNHash value object.

#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string>

#include "domain/shared/bn_hash.hpp"

using pvpgn::domain::BNHash;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("BNHash: default construction yields all-zero bytes",
          "[domain][shared][bn_hash]") {
    BNHash h;
    for (auto b : h.bytes()) {
        REQUIRE(b == 0u);
    }
}

TEST_CASE("BNHash: construct from Bytes array",
          "[domain][shared][bn_hash]") {
    BNHash::Bytes arr{};
    arr[0] = 0xDE;
    arr[19] = 0xAD;
    BNHash h{arr};
    REQUIRE(h.bytes()[0] == 0xDE);
    REQUIRE(h.bytes()[19] == 0xAD);
}

// ---------------------------------------------------------------------------
// from_bytes factory
// ---------------------------------------------------------------------------

TEST_CASE("BNHash: from_bytes accepts exactly 20 bytes",
          "[domain][shared][bn_hash]") {
    std::string raw(20, '\x42');
    auto result = BNHash::from_bytes(raw);
    REQUIRE(result.has_value());
    for (auto b : result.value().bytes()) {
        REQUIRE(b == 0x42u);
    }
}

TEST_CASE("BNHash: from_bytes rejects fewer than 20 bytes",
          "[domain][shared][bn_hash]") {
    std::string raw(19, '\x00');
    REQUIRE_FALSE(BNHash::from_bytes(raw).has_value());
}

TEST_CASE("BNHash: from_bytes rejects more than 20 bytes",
          "[domain][shared][bn_hash]") {
    std::string raw(21, '\x00');
    REQUIRE_FALSE(BNHash::from_bytes(raw).has_value());
}

TEST_CASE("BNHash: from_bytes rejects empty string",
          "[domain][shared][bn_hash]") {
    REQUIRE_FALSE(BNHash::from_bytes("").has_value());
}

// ---------------------------------------------------------------------------
// Equality (constant-time)
// ---------------------------------------------------------------------------

TEST_CASE("BNHash: equal hashes compare equal",
          "[domain][shared][bn_hash]") {
    auto a = BNHash::from_bytes(std::string(20, '\xAB')).value();
    auto b = BNHash::from_bytes(std::string(20, '\xAB')).value();
    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("BNHash: different hashes compare unequal",
          "[domain][shared][bn_hash]") {
    auto a = BNHash::from_bytes(std::string(20, '\x01')).value();
    auto b = BNHash::from_bytes(std::string(20, '\x02')).value();
    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}

TEST_CASE("BNHash: hashes differing in last byte compare unequal",
          "[domain][shared][bn_hash]") {
    std::string raw_a(20, '\x00');
    std::string raw_b(20, '\x00');
    raw_b[19] = '\x01';
    auto a = BNHash::from_bytes(raw_a).value();
    auto b = BNHash::from_bytes(raw_b).value();
    REQUIRE(a != b);
}

TEST_CASE("BNHash: default-constructed hashes are equal",
          "[domain][shared][bn_hash]") {
    BNHash a, b;
    REQUIRE(a == b);
}

// ---------------------------------------------------------------------------
// equals_constant_time
// ---------------------------------------------------------------------------

TEST_CASE("BNHash: equals_constant_time matches operator==",
          "[domain][shared][bn_hash]") {
    auto a = BNHash::from_bytes(std::string(20, '\x55')).value();
    auto b = BNHash::from_bytes(std::string(20, '\x55')).value();
    auto c = BNHash::from_bytes(std::string(20, '\xAA')).value();

    REQUIRE(a.equals_constant_time(b));
    REQUIRE_FALSE(a.equals_constant_time(c));
}

// ---------------------------------------------------------------------------
// bytes() size
// ---------------------------------------------------------------------------

TEST_CASE("BNHash: bytes() has exactly 20 elements",
          "[domain][shared][bn_hash]") {
    BNHash h;
    REQUIRE(h.bytes().size() == BNHash::kSize);
    REQUIRE(BNHash::kSize == 20u);
}
