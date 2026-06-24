// SPDX-License-Identifier: GPL-2.0-or-later

/// @file ip_address_exhaustive_test.cpp
/// Branch coverage for domain::IpAddress beyond ip_address_test.cpp.
/// Targets: error StatusCode on each rejection branch, IPv6 full-form parse /
/// family / to_string round-trip, IPv6 group-count and value-range rejection,
/// octet 255 vs 256 boundary, leading-empty / trailing-dot edge cases,
/// direct V6 construction, and v4 vs v6 inequality.
///
/// The parser is full-form only (no "::" compression) and to_string() emits
/// zero-padded 4-hex-digit groups, so round-trip tests use padded input.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/ip_address.hpp"

using pvpgn::core::StatusCode;
using pvpgn::domain::IpAddress;

// ---------------------------------------------------------------------------
// IPv4 error-code branches
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: octet > 255 yields InvalidArgument",
          "[domain][shared][ip_address]") {
    auto r = IpAddress::parse("256.0.0.1");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("IpAddress: 255 is accepted but 256 is rejected (boundary)",
          "[domain][shared][ip_address]") {
    REQUIRE(IpAddress::parse("255.255.255.255").has_value());
    REQUIRE_FALSE(IpAddress::parse("255.255.255.256").has_value());
}

TEST_CASE("IpAddress: empty octet (double dot) is rejected",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("192..0.1").has_value());
}

TEST_CASE("IpAddress: leading dot is rejected (no digit before first dot)",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse(".1.2.3").has_value());
}

TEST_CASE("IpAddress: trailing dot is rejected (no digit in final octet)",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("1.2.3.").has_value());
}

TEST_CASE("IpAddress: five octets are rejected with InvalidArgument",
          "[domain][shared][ip_address]") {
    auto r = IpAddress::parse("1.2.3.4.5");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("IpAddress: an unexpected character mid-octet is rejected",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse("1.2.3x.4").has_value());
}

TEST_CASE("IpAddress: octet of all zeros and large-but-valid octet round-trip",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("0.255.128.7").value();
    REQUIRE(ip.to_string() == "0.255.128.7");
    REQUIRE(ip.v4().at(1) == 255u);
}

// ---------------------------------------------------------------------------
// IPv6 — happy path
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: parse accepts full-form IPv6 and reports V6 family",
          "[domain][shared][ip_address]") {
    auto r = IpAddress::parse("2001:0db8:0000:0000:0000:0000:0000:0001");
    REQUIRE(r.has_value());
    auto ip = r.value();
    REQUIRE(ip.is_v6());
    REQUIRE_FALSE(ip.is_v4());
    REQUIRE(ip.family() == IpAddress::Family::V6);
}

TEST_CASE("IpAddress: IPv6 to_string round-trips zero-padded full form",
          "[domain][shared][ip_address]") {
    const std::string addr = "2001:0db8:0000:0000:0000:0000:0000:0001";
    auto ip = IpAddress::parse(addr).value();
    REQUIRE(ip.to_string() == addr);
}

TEST_CASE("IpAddress: IPv6 all-zero address round-trips",
          "[domain][shared][ip_address]") {
    const std::string addr = "0000:0000:0000:0000:0000:0000:0000:0000";
    auto ip = IpAddress::parse(addr).value();
    REQUIRE(ip.is_v6());
    REQUIRE(ip.to_string() == addr);
}

TEST_CASE("IpAddress: IPv6 accepts uppercase and lowercase hex digits",
          "[domain][shared][ip_address]") {
    auto lower = IpAddress::parse("00ab:00cd:0000:0000:0000:0000:0000:00ef");
    auto upper = IpAddress::parse("00AB:00CD:0000:0000:0000:0000:0000:00EF");
    REQUIRE(lower.has_value());
    REQUIRE(upper.has_value());
    // Hex is case-insensitive, so the two parse to the same address.
    REQUIRE(lower.value() == upper.value());
}

TEST_CASE("IpAddress: parsed IPv6 group bytes are big-endian within the group",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("0102:0000:0000:0000:0000:0000:0000:0000").value();
    REQUIRE(ip.v6().at(0) == 0x01u);
    REQUIRE(ip.v6().at(1) == 0x02u);
}

// ---------------------------------------------------------------------------
// IPv6 — rejection branches
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: IPv6 with too few groups is rejected",
          "[domain][shared][ip_address]") {
    auto r = IpAddress::parse("2001:db8:0:0:0:0:1");  // 7 groups
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("IpAddress: IPv6 with too many groups is rejected",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(
        IpAddress::parse("1:2:3:4:5:6:7:8:9").has_value());
}

TEST_CASE("IpAddress: IPv6 group > 0xFFFF is rejected",
          "[domain][shared][ip_address]") {
    auto r = IpAddress::parse("10000:0:0:0:0:0:0:1");  // first group overflows
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().code() == StatusCode::InvalidArgument);
}

TEST_CASE("IpAddress: IPv6 with empty group (::-style compression) is rejected",
          "[domain][shared][ip_address]") {
    // The parser explicitly does NOT support "::" compression.
    REQUIRE_FALSE(IpAddress::parse("2001::1").has_value());
}

TEST_CASE("IpAddress: IPv6 with a non-hex character is rejected",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(
        IpAddress::parse("2001:0gb8:0000:0000:0000:0000:0000:0001").has_value());
}

TEST_CASE("IpAddress: a lone colon is routed to the v6 parser and rejected",
          "[domain][shared][ip_address]") {
    REQUIRE_FALSE(IpAddress::parse(":").has_value());
}

// ---------------------------------------------------------------------------
// Direct construction and cross-family comparison
// ---------------------------------------------------------------------------

TEST_CASE("IpAddress: construct directly from a V6 array",
          "[domain][shared][ip_address]") {
    IpAddress::V6 arr{};
    arr[15] = 1;
    IpAddress ip{arr};
    REQUIRE(ip.is_v6());
    REQUIRE(ip.to_string() == "0000:0000:0000:0000:0000:0000:0000:0001");
}

TEST_CASE("IpAddress: a V4 address never equals a V6 address",
          "[domain][shared][ip_address]") {
    auto v4 = IpAddress::parse("0.0.0.1").value();
    auto v6 = IpAddress::parse("0000:0000:0000:0000:0000:0000:0000:0001").value();
    REQUIRE(v4 != v6);
    REQUIRE_FALSE(v4 == v6);
}

TEST_CASE("IpAddress: v4_packed reflects each octet position",
          "[domain][shared][ip_address]") {
    auto ip = IpAddress::parse("1.2.3.4").value();
    REQUIRE(ip.v4_packed() == 0x01020304u);
}

TEST_CASE("IpAddress: constexpr default construction yields a V4 zero address",
          "[domain][shared][ip_address]") {
    constexpr IpAddress ip;  // default ctor is constexpr
    REQUIRE(ip.is_v4());
}
