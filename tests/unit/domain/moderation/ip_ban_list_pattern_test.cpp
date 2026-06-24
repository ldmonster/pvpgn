// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for IpBanList integration of all five bnban.conf ban forms:
// wildcard / inclusive-range patterns plus the reason/expiry recovery
// (`match_info`) used by the application layer for accurate ban messages.

#include <chrono>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/moderation/ban_pattern.hpp"
#include "domain/moderation/ip_ban_list.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::IpAddress;
using domain::moderation::BanPattern;
using domain::moderation::IpBanEntry;
using domain::moderation::IpBanList;

namespace {
const auto t0 = std::chrono::system_clock::time_point{};
const auto t_plus_1h = t0 + std::chrono::hours(1);
const auto t_plus_2h = t0 + std::chrono::hours(2);

IpAddress ip(const char* s) { return IpAddress::parse(s).value(); }
BanPattern pat(const char* s) { return BanPattern::parse(s).value(); }
}  // namespace

TEST_CASE("IpBanList: wildcard pattern blocks matching hosts only",
          "[domain][moderation][ipban][pattern]") {
    IpBanList lst;
    REQUIRE(lst.add_ban_pattern(pat("1.2.*.4"), "wild", AccountId{1}, t0));
    REQUIRE(lst.pattern_count() == 1);

    REQUIRE(lst.blocks(ip("1.2.9.4"), t0));
    REQUIRE_FALSE(lst.blocks(ip("1.2.9.5"), t0));

    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanRangeAdded>(evs[0]));
}

TEST_CASE("IpBanList: inclusive range pattern boundaries",
          "[domain][moderation][ipban][pattern]") {
    IpBanList lst;
    REQUIRE(lst.add_ban_pattern(pat("1.2.3.4-1.2.3.40"), "range", AccountId{1}, t0));
    REQUIRE(lst.blocks(ip("1.2.3.20"), t0));
    REQUIRE_FALSE(lst.blocks(ip("1.2.3.41"), t0));
    REQUIRE_FALSE(lst.blocks(ip("1.2.3.3"), t0));
}

TEST_CASE("IpBanList: add_ban_pattern rejects exact/CIDR forms",
          "[domain][moderation][ipban][pattern]") {
    IpBanList lst;
    REQUIRE_FALSE(lst.add_ban_pattern(pat("1.2.3.4"), "x", AccountId{1}, t0));
    REQUIRE_FALSE(lst.add_ban_pattern(pat("10.0.0.0/8"), "x", AccountId{1}, t0));
    REQUIRE(lst.pattern_count() == 0);
    REQUIRE(lst.drain_events().empty());
}

TEST_CASE("IpBanList: expired pattern is skipped then pruned",
          "[domain][moderation][ipban][pattern]") {
    IpBanList lst;
    REQUIRE(lst.add_ban_pattern(pat("1.2.3.*"), "tmp", AccountId{1}, t0, t_plus_1h));
    (void)lst.drain_events();

    REQUIRE(lst.blocks(ip("1.2.3.99"), t0));
    REQUIRE_FALSE(lst.blocks(ip("1.2.3.99"), t_plus_2h));
    REQUIRE(lst.pattern_count() == 1);

    REQUIRE(lst.prune_expired(t_plus_2h) == 1);
    REQUIRE(lst.pattern_count() == 0);
    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanRangeRemoved>(evs[0]));
}

TEST_CASE("IpBanList: match_info recovers reason/expiry for each form",
          "[domain][moderation][ipban][pattern]") {
    IpBanList lst;
    lst.add(IpBanEntry{ip("9.9.9.9"), "exact-reason", AccountId{1}, t0, t_plus_1h});
    lst.add_range(ip("10.0.0.0"), 8, "cidr-reason", AccountId{1}, t0, t_plus_2h);
    lst.add_ban_pattern(pat("1.2.*.4"), "wild-reason", AccountId{1}, t0);
    lst.add_ban_pattern(pat("172.16.0.1-172.16.0.9"), "range-reason",
                        AccountId{1}, t0);

    // exact
    auto e = lst.match_info(ip("9.9.9.9"), t0);
    REQUIRE(e);
    REQUIRE(e->reason == "exact-reason");
    REQUIRE(e->expires_at == t_plus_1h);

    // cidr
    auto c = lst.match_info(ip("10.5.5.5"), t0);
    REQUIRE(c);
    REQUIRE(c->reason == "cidr-reason");
    REQUIRE(c->expires_at == t_plus_2h);

    // wildcard
    auto w = lst.match_info(ip("1.2.250.4"), t0);
    REQUIRE(w);
    REQUIRE(w->reason == "wild-reason");
    REQUIRE_FALSE(w->expires_at.has_value());

    // inclusive range
    auto r = lst.match_info(ip("172.16.0.5"), t0);
    REQUIRE(r);
    REQUIRE(r->reason == "range-reason");

    // no match
    REQUIRE_FALSE(lst.match_info(ip("8.8.8.8"), t0).has_value());
}
