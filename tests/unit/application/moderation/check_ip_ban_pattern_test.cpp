// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests that `application::moderation::CheckIpBan` reports the correct
// reason/expiry when an IP is blocked by a *non-exact* form (CIDR range,
// wildcard, inclusive range). Previously the use-case only scanned exact
// host entries, so range-matched IPs were mis-reported as a permanent
// "Banned" with no reason. The repository now exposes the full ban list
// via `load_banlist()`, and the use-case recovers reason/expiry from the
// matching entry across all forms.

#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "application/moderation/check_ip_ban.hpp"
#include "core/clock.hpp"
#include "domain/moderation/ban_pattern.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace {

using namespace pvpgn;
using application::moderation::CheckIpBan;
using domain::moderation::BanPattern;
using domain::moderation::IpBanList;

domain::IpAddress make_ip(std::string_view s) {
    return domain::IpAddress::parse(s).value();
}

/// Repository backed by a real IpBanList aggregate, so is_banned() and
/// load_banlist() agree across all ban forms (the realistic contract that
/// the inmemory / SQL repositories satisfy).
class BanlistBackedRepo : public domain::moderation::IIpBanRepository {
public:
    IpBanList banlist;

    core::Result<bool> is_banned(const domain::IpAddress& ip) const override {
        return banlist.blocks(ip, std::chrono::system_clock::now());
    }
    core::Status<> add_ban(domain::moderation::IpBanEntry e) override {
        banlist.add(std::move(e));
        return core::ok();
    }
    core::Status<> add_range_ban(domain::IpAddress, std::uint8_t, std::string,
                                 domain::AccountId, core::SystemTime,
                                 std::optional<core::SystemTime>) override {
        return core::ok();
    }
    core::Status<> remove_ban(const domain::IpAddress&) override {
        return core::ok();
    }
    core::Status<> remove_range_ban(domain::IpAddress, std::uint8_t) override {
        return core::ok();
    }
    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> pred)
        const override {
        for (const auto& e : banlist.entries()) {
            if (!pred(e)) return;
        }
    }
    core::Result<IpBanList> load_banlist() const override { return banlist; }
    core::Status<> save_banlist(const IpBanList&) override { return core::ok(); }
};

const auto t0 = std::chrono::system_clock::now();

}  // namespace

TEST_CASE("CheckIpBan: CIDR-range match reports the range's reason/expiry",
          "[application][moderation][ipban][pattern]") {
    BanlistBackedRepo repo;
    const auto exp = t0 + std::chrono::hours(5);
    repo.banlist.add_range(make_ip("10.0.0.0"), 8, "subnet-abuse",
                           domain::AccountId{1}, t0, exp);

    CheckIpBan uc{repo};
    auto r = uc.execute(make_ip("10.4.5.6"));

    REQUIRE(r);
    REQUIRE(r.value().banned);
    REQUIRE(r.value().reason == "subnet-abuse");
    REQUIRE(r.value().expires_at.has_value());
    REQUIRE(*r.value().expires_at == exp);
}

TEST_CASE("CheckIpBan: middle-octet wildcard match reports its reason",
          "[application][moderation][ipban][pattern]") {
    BanlistBackedRepo repo;
    repo.banlist.add_ban_pattern(BanPattern::parse("1.2.*.4").value(),
                                 "wild-abuse", domain::AccountId{1}, t0);

    CheckIpBan uc{repo};

    auto hit = uc.execute(make_ip("1.2.99.4"));
    REQUIRE(hit);
    REQUIRE(hit.value().banned);
    REQUIRE(hit.value().reason == "wild-abuse");

    auto miss = uc.execute(make_ip("1.2.99.5"));
    REQUIRE(miss);
    REQUIRE_FALSE(miss.value().banned);
}

TEST_CASE("CheckIpBan: inclusive-range match reports its reason",
          "[application][moderation][ipban][pattern]") {
    BanlistBackedRepo repo;
    repo.banlist.add_ban_pattern(
        BanPattern::parse("1.2.3.4-1.2.3.40").value(), "range-abuse",
        domain::AccountId{1}, t0);

    CheckIpBan uc{repo};

    auto hit = uc.execute(make_ip("1.2.3.20"));
    REQUIRE(hit);
    REQUIRE(hit.value().banned);
    REQUIRE(hit.value().reason == "range-abuse");

    auto miss = uc.execute(make_ip("1.2.3.41"));
    REQUIRE(miss);
    REQUIRE_FALSE(miss.value().banned);
}
