// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "integration/legacy_bnetd/legacy_account_hex.hpp"

using pvpgn::integration::legacy_bnetd::detail::hex_decode_passhash;
using pvpgn::integration::legacy_bnetd::detail::hex_encode_passhash;
using pvpgn::domain::BNHash;

TEST_CASE("legacy passhash hex codec round-trips known vector",
          "[integration][legacy_bnetd][hex]") {
    // Known vector: bytes 00, 01, 02, ..., 13 -> "000102030405060708090a0b0c0d0e0f10111213"
    BNHash::Bytes bytes{};
    for (std::size_t i = 0; i < BNHash::kSize; ++i) {
        bytes[i] = static_cast<std::uint8_t>(i);
    }
    BNHash original{bytes};
    const std::string hex = hex_encode_passhash(original);
    REQUIRE(hex == "000102030405060708090a0b0c0d0e0f10111213");

    auto decoded = hex_decode_passhash(hex);
    REQUIRE(decoded.has_value());
    REQUIRE(BNHash{*decoded} == original);
}

TEST_CASE("legacy passhash hex codec accepts uppercase",
          "[integration][legacy_bnetd][hex]") {
    auto decoded = hex_decode_passhash(
        "ABCDEF0123456789ABCDEF0123456789ABCDEF01");
    REQUIRE(decoded.has_value());
    // Re-encode is always lowercase.
    const std::string re = hex_encode_passhash(BNHash{*decoded});
    REQUIRE(re == "abcdef0123456789abcdef0123456789abcdef01");
}

TEST_CASE("legacy passhash hex codec rejects malformed input",
          "[integration][legacy_bnetd][hex]") {
    REQUIRE_FALSE(hex_decode_passhash("").has_value());
    REQUIRE_FALSE(hex_decode_passhash("deadbeef").has_value());      // too short
    REQUIRE_FALSE(hex_decode_passhash(
        std::string(41, '0')).has_value());                          // too long
    REQUIRE_FALSE(hex_decode_passhash(
        "g00102030405060708090a0b0c0d0e0f10111213").has_value());    // non-hex char
}
