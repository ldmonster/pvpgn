// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new coverage for `domain::moderation::IpBanList`. Targets the
// branches not exercised by ip_ban_list_test.cpp: rehydrate/entries
// round-trip, multi-entry iteration, prefix-bit clamping, IPv6 exact +
// CIDR bans, cross-family range mismatch, remove_range miss/hit,
// expired-range "continue" branch in blocks(), and the two-event
// duplicate-replace sequence.

#include <chrono>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/moderation/ip_ban_list.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::IpAddress;
using domain::moderation::IpBanEntry;
using domain::moderation::IpBanList;

namespace {
const auto t0 = std::chrono::system_clock::time_point{};
const auto t_plus_1h = t0 + std::chrono::hours(1);
const auto t_plus_2h = t0 + std::chrono::hours(2);

IpAddress ip(const char* s) { return IpAddress::parse(s).value(); }

IpBanEntry make_entry(const char* a,
                      std::optional<core::SystemTime> exp = std::nullopt) {
    return IpBanEntry{ip(a), "spam", AccountId{7}, t0, exp};
}
}  // namespace

TEST_CASE("IpBanList: default-constructed list is empty",
          "[domain][moderation]") {
    IpBanList lst;
    REQUIRE(lst.size() == 0);
    REQUIRE(lst.range_count() == 0);
    REQUIRE(lst.entries().empty());
    REQUIRE_FALSE(lst.blocks(ip("10.0.0.1"), t0));
    REQUIRE(lst.drain_events().empty());
}

TEST_CASE("IpBanList: rehydrate restores entries without emitting events",
          "[domain][moderation]") {
    std::vector<IpBanEntry> seed{
        make_entry("10.0.0.1"),
        make_entry("10.0.0.2", t_plus_1h),
    };
    auto lst = IpBanList::rehydrate(seed);

    REQUIRE(lst.size() == 2);
    REQUIRE(lst.entries().size() == 2);
    REQUIRE(lst.entries()[0].ip == ip("10.0.0.1"));
    REQUIRE(lst.entries()[1].expires_at.has_value());
    // rehydrate is a load path: no domain events recorded.
    REQUIRE(lst.drain_events().empty());
    REQUIRE(lst.blocks(ip("10.0.0.1"), t0));
}

TEST_CASE("IpBanList: entries() exposes every added ban for iteration",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    lst.add(make_entry("10.0.0.2"));
    lst.add(make_entry("10.0.0.3"));
    (void)lst.drain_events();

    REQUIRE(lst.size() == 3);
    std::size_t seen = 0;
    for (const auto& e : lst.entries()) {
        REQUIRE(e.reason == "spam");
        ++seen;
    }
    REQUIRE(seen == 3);
}

TEST_CASE("IpBanList: duplicate add emits removal + addition events",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    lst.add(make_entry("10.0.0.1", t_plus_1h));

    auto evs = lst.drain_events();
    // First add: one IpBanAdded. Second add: replace silently removes the
    // old entry (no IpBanRemoved event from add()), then a second
    // IpBanAdded. So exactly two events, both additions.
    REQUIRE(evs.size() == 2);
    REQUIRE(std::holds_alternative<domain::events::IpBanAdded>(evs[0]));
    REQUIRE(std::holds_alternative<domain::events::IpBanAdded>(evs[1]));
    REQUIRE(lst.size() == 1);
}

TEST_CASE("IpBanList: remove emits IpBanRemoved on hit",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    (void)lst.drain_events();

    REQUIRE(lst.remove(ip("10.0.0.1")));
    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanRemoved>(evs[0]));
}

TEST_CASE("IpBanList: remove miss emits no event",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    (void)lst.drain_events();

    REQUIRE_FALSE(lst.remove(ip("10.0.0.9")));
    REQUIRE(lst.drain_events().empty());
    REQUIRE(lst.size() == 1);
}

TEST_CASE("IpBanList: add_range clamps prefix_bits above the v4 cap",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    // 99 > 32: clamped to /32, so only the exact host matches.
    lst.add_range(ip("10.1.2.3"), 99, "clamped", AccountId{1}, t0);
    REQUIRE(lst.range_count() == 1);
    REQUIRE(lst.blocks(ip("10.1.2.3"), t0));
    REQUIRE_FALSE(lst.blocks(ip("10.1.2.4"), t0));
}

TEST_CASE("IpBanList: /0 range blocks every v4 host",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(ip("0.0.0.0"), 0, "all", AccountId{1}, t0);
    REQUIRE(lst.blocks(ip("1.2.3.4"), t0));
    REQUIRE(lst.blocks(ip("255.255.255.255"), t0));
}

TEST_CASE("IpBanList: exact IPv6 ban blocks only that host",
          "[domain][moderation]") {
    IpBanList lst;
    const char* v6 = "fe80:0000:0000:0000:0000:0000:0000:0001";
    lst.add(make_entry(v6));
    (void)lst.drain_events();

    REQUIRE(lst.blocks(ip(v6), t0));
    REQUIRE_FALSE(lst.blocks(ip("fe80:0000:0000:0000:0000:0000:0000:0002"), t0));
}

TEST_CASE("IpBanList: IPv6 CIDR /64 blocks hosts sharing the prefix",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(ip("2001:0db8:0000:0000:0000:0000:0000:0000"), 64,
                  "v6subnet", AccountId{1}, t0);
    REQUIRE(lst.blocks(ip("2001:0db8:0000:0000:dead:beef:0000:0001"), t0));
    REQUIRE_FALSE(lst.blocks(ip("2001:0db8:0001:0000:0000:0000:0000:0001"), t0));
}

TEST_CASE("IpBanList: a v4 range never matches a v6 candidate",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(ip("10.0.0.0"), 8, "v4only", AccountId{1}, t0);
    // Family mismatch path: range_matches_ returns false up front.
    REQUIRE_FALSE(lst.blocks(ip("0a00:0000:0000:0000:0000:0000:0000:0000"), t0));
}

TEST_CASE("IpBanList: remove_range returns false on miss, true on exact hit",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(ip("10.0.0.0"), 16, "x", AccountId{1}, t0);
    (void)lst.drain_events();

    // Wrong prefix length: not an exact match.
    REQUIRE_FALSE(lst.remove_range(ip("10.0.0.0"), 24));
    // Wrong network: not an exact match.
    REQUIRE_FALSE(lst.remove_range(ip("11.0.0.0"), 16));
    REQUIRE(lst.range_count() == 1);

    REQUIRE(lst.remove_range(ip("10.0.0.0"), 16));
    REQUIRE(lst.range_count() == 0);
    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanRangeRemoved>(evs[0]));
}

TEST_CASE("IpBanList: expired range is skipped by blocks() but kept until prune",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(ip("10.0.0.0"), 8, "tmp", AccountId{1}, t0, t_plus_1h);
    (void)lst.drain_events();

    REQUIRE(lst.blocks(ip("10.5.5.5"), t0));
    // Past expiry: blocks() hits the `continue` branch and returns false,
    // but the range entry is still present (not yet pruned).
    REQUIRE_FALSE(lst.blocks(ip("10.5.5.5"), t_plus_2h));
    REQUIRE(lst.range_count() == 1);
}

TEST_CASE("IpBanList: prune_expired removes both stale entries and ranges",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1", t_plus_1h));        // expires
    lst.add(make_entry("10.0.0.2"));                   // permanent
    lst.add_range(ip("172.16.0.0"), 12, "r", AccountId{1}, t0, t_plus_1h);
    lst.add_range(ip("192.168.0.0"), 16, "r2", AccountId{1}, t0);
    (void)lst.drain_events();

    REQUIRE(lst.prune_expired(t_plus_2h) == 2);
    REQUIRE(lst.size() == 1);
    REQUIRE(lst.range_count() == 1);
    REQUIRE(lst.entries().front().ip == ip("10.0.0.2"));

    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 2);  // one IpBanRemoved + one IpBanRangeRemoved
}

TEST_CASE("IpBanList: prune_expired removes nothing when all bans are active",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    lst.add_range(ip("10.0.0.0"), 8, "r", AccountId{1}, t0);
    (void)lst.drain_events();

    REQUIRE(lst.prune_expired(t_plus_2h) == 0);
    REQUIRE(lst.size() == 1);
    REQUIRE(lst.range_count() == 1);
    REQUIRE(lst.drain_events().empty());
}

TEST_CASE("IpBanEntry::active_at honours expiry boundary",
          "[domain][moderation]") {
    IpBanEntry permanent = make_entry("10.0.0.1");
    REQUIRE(permanent.active_at(t0));
    REQUIRE(permanent.active_at(t_plus_2h));

    IpBanEntry temporary = make_entry("10.0.0.2", t_plus_1h);
    REQUIRE(temporary.active_at(t0));
    // active_at is strict: now == expires_at is no longer active.
    REQUIRE_FALSE(temporary.active_at(t_plus_1h));
    REQUIRE_FALSE(temporary.active_at(t_plus_2h));
}
