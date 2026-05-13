// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

using namespace pvpgn;

TEST_CASE("ClientTag: parses 4-byte printable ASCII", "[domain][shared]") {
    auto t = domain::ClientTag::parse("STAR");
    REQUIRE(t.has_value());
    REQUIRE(std::string{t.value().text()} == "STAR");
    REQUIRE(t.value().packed_be() == 0x53544152u);  // 'S','T','A','R'
}

TEST_CASE("ClientTag: rejects wrong length / non-printable", "[domain][shared]") {
    REQUIRE_FALSE(domain::ClientTag::parse("STA").has_value());
    REQUIRE_FALSE(domain::ClientTag::parse("STARS").has_value());
    REQUIRE_FALSE(domain::ClientTag::parse(std::string_view{"ST\x01R", 4}).has_value());
}

TEST_CASE("UserName: accepts canonical names, rejects garbage", "[domain][shared]") {
    REQUIRE(domain::UserName::parse("Alice").has_value());
    REQUIRE(domain::UserName::parse("a.b-c_d").has_value());
    REQUIRE_FALSE(domain::UserName::parse("a").has_value());                  // too short
    REQUIRE_FALSE(domain::UserName::parse("1234").has_value());               // leading digit
    REQUIRE_FALSE(domain::UserName::parse("has space").has_value());          // space
    REQUIRE_FALSE(domain::UserName::parse("waaaaaaaaaaaaaaay").has_value()); // too long
}

TEST_CASE("UserName: case-insensitive equality, case-preserving display",
          "[domain][shared]") {
    auto a = domain::UserName::parse("Alice").value();
    auto b = domain::UserName::parse("ALICE").value();
    REQUIRE(a == b);
    REQUIRE(std::string{a.display()}   == "Alice");
    REQUIRE(std::string{a.canonical()} == "alice");
}

TEST_CASE("Locale: parses canonical xxYY, falls back to enUS", "[domain][shared]") {
    auto l = domain::Locale::parse_or_default("deDE");
    REQUIRE(std::string{l.text()} == "deDE");
    REQUIRE_FALSE(l.is_default());
    REQUIRE(domain::Locale::parse_or_default("garbage").is_default());
    REQUIRE(std::string{domain::Locale{}.text()} == "enUS");
}

TEST_CASE("BNHash: requires exactly 20 bytes; constant-time equality",
          "[domain][shared]") {
    std::string twenty(20, '\x42');
    auto h = domain::BNHash::from_bytes(twenty);
    REQUIRE(h.has_value());
    auto h2 = domain::BNHash::from_bytes(twenty);
    REQUIRE(h2.has_value());
    REQUIRE(h.value() == h2.value());
    REQUIRE_FALSE(domain::BNHash::from_bytes(std::string(19, 'x')).has_value());
}

TEST_CASE("IpAddress: parses dotted-quad", "[domain][shared][ip]") {
    auto a = domain::IpAddress::parse("192.168.0.1");
    REQUIRE(a.has_value());
    REQUIRE(a.value().is_v4());
    REQUIRE(a.value().v4_packed() == 0xC0A80001u);
    REQUIRE(a.value().to_string() == "192.168.0.1");
}

TEST_CASE("IpAddress: parses full IPv6", "[domain][shared][ip]") {
    auto a = domain::IpAddress::parse("2001:0db8:0000:0000:0000:0000:0000:0001");
    REQUIRE(a.has_value());
    REQUIRE(a.value().is_v6());
    REQUIRE(a.value().to_string() == "2001:0db8:0000:0000:0000:0000:0000:0001");
}

TEST_CASE("IpAddress: rejects malformed input", "[domain][shared][ip]") {
    REQUIRE_FALSE(domain::IpAddress::parse("999.0.0.1").has_value());
    REQUIRE_FALSE(domain::IpAddress::parse("1.2.3").has_value());
    REQUIRE_FALSE(domain::IpAddress::parse("xyz").has_value());
    REQUIRE_FALSE(domain::IpAddress::parse("1::1").has_value());  // compression not supported
}
