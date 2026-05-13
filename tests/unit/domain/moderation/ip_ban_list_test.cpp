// SPDX-License-Identifier: GPL-2.0-or-later
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

IpBanEntry make_entry(const char* ip, std::optional<core::SystemTime> exp = std::nullopt) {
    return IpBanEntry{IpAddress::parse(ip).value(), "spam",
                      AccountId{1}, t0, exp};
}
}  // namespace

TEST_CASE("IpBanList: add then blocks query",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    REQUIRE(lst.blocks(IpAddress::parse("10.0.0.1").value(), t0));
    REQUIRE_FALSE(lst.blocks(IpAddress::parse("10.0.0.2").value(), t0));
    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanAdded>(evs[0]));
}

TEST_CASE("IpBanList: duplicate add replaces (single entry, two events)",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    lst.add(make_entry("10.0.0.1", t_plus_1h));
    REQUIRE(lst.size() == 1);
    REQUIRE(lst.entries().front().expires_at.has_value());
}

TEST_CASE("IpBanList: expired entry no longer blocks; prune removes it",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1", t_plus_1h));
    (void)lst.drain_events();

    REQUIRE(lst.blocks(IpAddress::parse("10.0.0.1").value(), t0));
    REQUIRE_FALSE(lst.blocks(IpAddress::parse("10.0.0.1").value(), t_plus_2h));

    REQUIRE(lst.prune_expired(t_plus_2h) == 1);
    REQUIRE(lst.size() == 0);
    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanRemoved>(evs[0]));
}

TEST_CASE("IpBanList: remove returns false on miss, true on hit",
          "[domain][moderation]") {
    IpBanList lst;
    lst.add(make_entry("10.0.0.1"));
    (void)lst.drain_events();
    REQUIRE_FALSE(lst.remove(IpAddress::parse("10.0.0.9").value()));
    REQUIRE(lst.remove(IpAddress::parse("10.0.0.1").value()));
    REQUIRE(lst.size() == 0);
}

#include "domain/moderation/quota.hpp"

using domain::moderation::Quota;
using domain::moderation::QuotaPolicy;

TEST_CASE("IpBanList: CIDR /24 range blocks every host in the network",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(IpAddress::parse("10.1.2.0").value(), 24, "subnet",
                  AccountId{1}, t0);
    REQUIRE(lst.range_count() == 1);
    REQUIRE(lst.blocks(IpAddress::parse("10.1.2.0").value(), t0));
    REQUIRE(lst.blocks(IpAddress::parse("10.1.2.99").value(), t0));
    REQUIRE(lst.blocks(IpAddress::parse("10.1.2.255").value(), t0));
    REQUIRE_FALSE(lst.blocks(IpAddress::parse("10.1.3.0").value(), t0));

    auto evs = lst.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::IpBanRangeAdded>(evs[0]));
}

TEST_CASE("IpBanList: range with /32 matches exactly one host",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(IpAddress::parse("10.1.2.3").value(), 32, "single",
                  AccountId{1}, t0);
    REQUIRE(lst.blocks(IpAddress::parse("10.1.2.3").value(), t0));
    REQUIRE_FALSE(lst.blocks(IpAddress::parse("10.1.2.4").value(), t0));
}

TEST_CASE("IpBanList: remove_range / prune_expired (ranges)",
          "[domain][moderation][cidr]") {
    IpBanList lst;
    lst.add_range(IpAddress::parse("10.0.0.0").value(), 16, "x",
                  AccountId{1}, t0, t_plus_1h);
    (void)lst.drain_events();
    REQUIRE(lst.blocks(IpAddress::parse("10.0.5.5").value(), t0));
    REQUIRE_FALSE(lst.blocks(IpAddress::parse("10.0.5.5").value(), t_plus_2h));
    REQUIRE(lst.prune_expired(t_plus_2h) == 1);
    REQUIRE(lst.range_count() == 0);
}

TEST_CASE("Quota: under-limit messages are Allowed",
          "[domain][moderation][quota]") {
    Quota q{AccountId{1}, QuotaPolicy{5, std::chrono::seconds(2), std::chrono::seconds(30)}};
    auto now = t0;
    for (int i = 0; i < 5; ++i) {
        REQUIRE(q.record(now) == Quota::Outcome::Allowed);
        now += std::chrono::milliseconds(100);
    }
    REQUIRE(q.drain_events().empty());
}

TEST_CASE("Quota: exceeding the limit throttles + mutes + emits event",
          "[domain][moderation][quota]") {
    Quota q{AccountId{1}, QuotaPolicy{3, std::chrono::seconds(2), std::chrono::seconds(30)}};
    REQUIRE(q.record(t0) == Quota::Outcome::Allowed);
    REQUIRE(q.record(t0 + std::chrono::milliseconds(100)) == Quota::Outcome::Allowed);
    REQUIRE(q.record(t0 + std::chrono::milliseconds(200)) == Quota::Outcome::Allowed);
    REQUIRE(q.record(t0 + std::chrono::milliseconds(300)) == Quota::Outcome::Throttled);

    REQUIRE(q.is_muted(t0 + std::chrono::milliseconds(400)));
    REQUIRE(q.record(t0 + std::chrono::milliseconds(400)) == Quota::Outcome::Muted);

    auto evs = q.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::AccountQuotaExceeded>(evs[0]));
}

TEST_CASE("Quota: mute lifts after mute_for elapses",
          "[domain][moderation][quota]") {
    Quota q{AccountId{1}, QuotaPolicy{1, std::chrono::seconds(2), std::chrono::seconds(5)}};
    (void)q.record(t0);
    (void)q.record(t0 + std::chrono::milliseconds(100));  // throttle → muted
    REQUIRE(q.is_muted(t0 + std::chrono::seconds(1)));
    REQUIRE_FALSE(q.is_muted(t0 + std::chrono::seconds(6)));
    REQUIRE(q.record(t0 + std::chrono::seconds(7)) == Quota::Outcome::Allowed);
}
