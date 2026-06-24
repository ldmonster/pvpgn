// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `domain::moderation::BanPattern` — the value type that models
// the five legacy bnban.conf IPv4 ban forms (exact, wildcard, inclusive
// range, CIDR prefix, dotted netmask). Each form must match the right IPs
// and reject the wrong ones, mirroring the original `ipbanlist_check`.

#include <catch2/catch_test_macros.hpp>

#include "domain/moderation/ban_pattern.hpp"
#include "domain/shared/ip_address.hpp"

using namespace pvpgn;
using domain::IpAddress;
using domain::moderation::BanPattern;

namespace {
std::uint32_t host(const char* s) { return IpAddress::parse(s).value().v4_packed(); }
}  // namespace

TEST_CASE("BanPattern: exact form matches only that host",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("1.2.3.4");
    REQUIRE(p);
    REQUIRE(p->kind() == BanPattern::Kind::Exact);
    REQUIRE(p->matches(host("1.2.3.4")));
    REQUIRE_FALSE(p->matches(host("1.2.3.5")));
    REQUIRE_FALSE(p->matches(host("1.2.4.4")));
}

TEST_CASE("BanPattern: trailing wildcard 1.2.3.* matches the /24, not the next net",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("1.2.3.*");
    REQUIRE(p);
    REQUIRE(p->kind() == BanPattern::Kind::Wildcard);
    REQUIRE(p->matches(host("1.2.3.0")));
    REQUIRE(p->matches(host("1.2.3.7")));
    REQUIRE(p->matches(host("1.2.3.255")));
    REQUIRE_FALSE(p->matches(host("1.2.4.7")));   // wrong third octet
    REQUIRE_FALSE(p->matches(host("9.2.3.7")));
}

TEST_CASE("BanPattern: middle-octet wildcard 1.2.*.4 matches 1.2.9.4 not 1.2.9.5",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("1.2.*.4");
    REQUIRE(p);
    REQUIRE(p->kind() == BanPattern::Kind::Wildcard);
    REQUIRE(p->matches(host("1.2.9.4")));
    REQUIRE(p->matches(host("1.2.0.4")));
    REQUIRE(p->matches(host("1.2.255.4")));
    REQUIRE_FALSE(p->matches(host("1.2.9.5")));   // wrong last octet
    REQUIRE_FALSE(p->matches(host("1.3.9.4")));   // wrong second octet
}

TEST_CASE("BanPattern: multi-octet wildcard 1.*.3.* matches per-octet",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("1.*.3.*");
    REQUIRE(p);
    REQUIRE(p->matches(host("1.50.3.99")));
    REQUIRE(p->matches(host("1.0.3.0")));
    REQUIRE_FALSE(p->matches(host("1.50.4.99")));  // third octet fixed at 3
    REQUIRE_FALSE(p->matches(host("2.50.3.99")));  // first octet fixed at 1
}

TEST_CASE("BanPattern: inclusive range matches .20 not .41",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("1.2.3.4-1.2.3.40");
    REQUIRE(p);
    REQUIRE(p->kind() == BanPattern::Kind::Range);
    REQUIRE(p->matches(host("1.2.3.4")));    // lo boundary inclusive
    REQUIRE(p->matches(host("1.2.3.20")));
    REQUIRE(p->matches(host("1.2.3.40")));   // hi boundary inclusive
    REQUIRE_FALSE(p->matches(host("1.2.3.3")));   // below lo
    REQUIRE_FALSE(p->matches(host("1.2.3.41")));  // above hi
}

TEST_CASE("BanPattern: range spanning octets",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("1.2.3.250-1.2.4.5");
    REQUIRE(p);
    REQUIRE(p->matches(host("1.2.3.255")));
    REQUIRE(p->matches(host("1.2.4.0")));
    REQUIRE(p->matches(host("1.2.4.5")));
    REQUIRE_FALSE(p->matches(host("1.2.4.6")));
    REQUIRE_FALSE(p->matches(host("1.2.3.249")));
}

TEST_CASE("BanPattern: CIDR prefix /24",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("10.0.0.0/24");
    REQUIRE(p);
    REQUIRE(p->kind() == BanPattern::Kind::Cidr);
    REQUIRE(p->matches(host("10.0.0.0")));
    REQUIRE(p->matches(host("10.0.0.255")));
    REQUIRE_FALSE(p->matches(host("10.0.1.0")));

    auto cidr = p->as_cidr();
    REQUIRE(cidr);
    REQUIRE(cidr->first == IpAddress::parse("10.0.0.0").value());
    REQUIRE(cidr->second == 24);
}

TEST_CASE("BanPattern: CIDR /0 matches everything (no >>32 UB)",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("0.0.0.0/0");
    REQUIRE(p);
    REQUIRE(p->matches(host("1.2.3.4")));
    REQUIRE(p->matches(host("255.255.255.255")));
}

TEST_CASE("BanPattern: CIDR /32 matches one host",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("8.8.8.8/32");
    REQUIRE(p);
    REQUIRE(p->matches(host("8.8.8.8")));
    REQUIRE_FALSE(p->matches(host("8.8.8.9")));
}

TEST_CASE("BanPattern: dotted netmask 255.255.255.0 == /24",
          "[domain][moderation][ipban][pattern]") {
    auto p = BanPattern::parse("192.168.1.0/255.255.255.0");
    REQUIRE(p);
    REQUIRE(p->kind() == BanPattern::Kind::Cidr);
    REQUIRE(p->matches(host("192.168.1.50")));
    REQUIRE_FALSE(p->matches(host("192.168.2.50")));
    auto cidr = p->as_cidr();
    REQUIRE(cidr);
    REQUIRE(cidr->second == 24);
}

TEST_CASE("BanPattern: non-contiguous netmask falls back to wildcard match",
          "[domain][moderation][ipban][pattern]") {
    // 255.0.255.0 is a legal (if exotic) original netmask; the original
    // simply ANDs both sides, so we match the masked bits directly.
    auto p = BanPattern::parse("1.0.3.0/255.0.255.0");
    REQUIRE(p);
    REQUIRE(p->matches(host("1.99.3.99")));   // 2nd + 4th octets are don't-care
    REQUIRE_FALSE(p->matches(host("2.99.3.99")));  // 1st octet must equal 1
    REQUIRE_FALSE(p->matches(host("1.99.4.99")));  // 3rd octet must equal 3
}

TEST_CASE("BanPattern: rejects malformed tokens",
          "[domain][moderation][ipban][pattern]") {
    REQUIRE_FALSE(BanPattern::parse(""));
    REQUIRE_FALSE(BanPattern::parse("1.2.3"));         // too few octets
    REQUIRE_FALSE(BanPattern::parse("1.2.3.4.5"));     // too many octets
    REQUIRE_FALSE(BanPattern::parse("1.2.3.256"));     // octet > 255
    REQUIRE_FALSE(BanPattern::parse("1.2.3.x"));       // non-numeric
    REQUIRE_FALSE(BanPattern::parse("1.2.3.4/33"));    // prefix > 32
    REQUIRE_FALSE(BanPattern::parse("1.2.3.*-1.2.3.4"));  // wildcard inside range
    REQUIRE_FALSE(BanPattern::parse("1.2.3.4/"));      // empty suffix
}
