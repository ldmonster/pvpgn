// SPDX-License-Identifier: GPL-2.0-or-later

/// @file ip_address_test.cpp
/// Unit tests for domain::IpAddress value object.

#include <catch2/catch_test_macros.hpp>

#include "domain/shared/ip_address.hpp"

using pvpgn::domain::IpAddress;

// ---------------------------------------------------------------------------
// Construction from valid IPv4 input
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: parse accepts valid IPv4 dotted-quad",
          "[domain][shared][ip_address]") {
    REQUIRE(IpAddress::parse("192.168.0.1").has_value());
    REQUIRE(IpAddress::parse("0.0.0.0").has_value());
    REQUIRE(IpAddress::parse("255.255.255.255").has_value());
    REQUIRE(IpAddress::parse("10.0.0.1").has_value());
    REQUIRE(IpAddress::parse("127.0.0.1").has_value());
}

TEST_CASE("IpAddress: parse IPv4 — family is V4",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("192.168.1.1").value();
    REQUIRE(ip.is_v4());
    REQUIRE_FALSE(ip.is_v6());
    REQUIRE(ip.family() == IpAddress::Family::V4);
}

// ---------------------------------------------------------------------------
// Rejection of invalid IPv4 input
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: parse rejects empty string",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("").has_value());
}

TEST_CASE("IpAddress: parse rejects IPv4 with out-of-range octet",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("256.0.0.1").has_value());
    REQUIRE_FALSE(IpAddress::parse("192.168.0.300").has_value());
}

TEST_CASE("IpAddress: parse rejects IPv4 with too few octets",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("192.168.0").has_value());
    REQUIRE_FALSE(IpAddress::parse("192.168").has_value());
}

TEST_CASE("IpAddress: parse rejects IPv4 with too many octets",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("192.168.0.1.5").has_value());
}

TEST_CASE("IpAddress: parse rejects non-numeric IPv4",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("abc.def.ghi.jkl").has_value());
}

// ---------------------------------------------------------------------------
// IPv4 packed value
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: v4_packed returns correct host-order value",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("192.168.0.1").value();
    // 192.168.0.1 → 0xC0A80001
    REQUIRE(ip.v4_packed() == 0xC0A80001u);
}

TEST_CASE("IpAddress: v4_packed for 0.0.0.0 is zero",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("0.0.0.0").value();
    REQUIRE(ip.v4_packed() == 0u);
}

TEST_CASE("IpAddress: v4_packed for 255.255.255.255 is max",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("255.255.255.255").value();
    REQUIRE(ip.v4_packed() == 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// to_string() round-trip
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: to_string round-trips IPv4",
          "[domain][shared][ip_address]") {
    const std::string addr = "10.20.30.40";
    auto ip = IpAddress::parse(addr).value();
    REQUIRE(ip.to_string() == addr);
}

TEST_CASE("IpAddress: to_string for 127.0.0.1",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("127.0.0.1").value();
    REQUIRE(ip.to_string() == "127.0.0.1");
}

// ---------------------------------------------------------------------------
// Equality comparison
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: equal addresses compare equal",
          "[domain][shared][ip_address]") {
    auto a = IpAddress::parse("10.0.0.1").value();
    auto b = IpAddress::parse("10.0.0.1").value();
    REQUIRE(a == b);
    REQUIRE_FALSE(a != b);
}

TEST_CASE("IpAddress: different addresses compare unequal",
          "[domain][shared][ip_address]") {
    auto a = IpAddress::parse("10.0.0.1").value();
    auto b = IpAddress::parse("10.0.0.2").value();
    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
}

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: default-constructed is 0.0.0.0",
          "[domain][shared][ip_address]") {
    IpAddress ip;
    REQUIRE(ip.is_v4());
    REQUIRE(ip.v4_packed() == 0u);
    REQUIRE(ip.to_string() == "0.0.0.0");
}

// ---------------------------------------------------------------------------
// Direct V4 construction
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: construct from V4 array",
          "[domain][shared][ip_address]") {
    IpAddress::V4 arr{192, 168, 1, 100};
    IpAddress ip{arr};
    REQUIRE(ip.is_v4());
    REQUIRE(ip.to_string() == "192.168.1.100");
}
