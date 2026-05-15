// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::CheckIpBan`. Exercises the use-case
// for IP ban checking during authentication.

#include <catch2/catch_test_macros.hpp>

#include "application/moderation/check_ip_ban.hpp"
#include "core/clock.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace {

using namespace pvpgn;
using application::moderation::CheckIpBan;

domain::IpAddress make_ip(std::string_view s) {
    auto r = domain::IpAddress::parse(s);
    REQUIRE(r);
    return r.value();
}

// Mock IP ban repository for testing
class MockIpBanRepository : public application::ports::IIpBanRepository {
public:
    core::Result<bool> is_banned(const domain::IpAddress&) const override {
        return false;  // Default: not banned
    }

    core::Status<> add_ban(domain::moderation::IpBanEntry) override {
        return core::ok();
    }

    core::Status<> add_range_ban(domain::IpAddress, std::uint8_t,
                                 std::string, domain::AccountId,
                                 core::SystemTime,
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
        std::function<bool(const domain::moderation::IpBanEntry&)>) const override {
    }

    core::Result<domain::moderation::IpBanList> load_banlist() const override {
        return domain::moderation::IpBanList::create().value();
    }

    core::Status<> save(const domain::moderation::IpBanList&) override {
        return core::ok();
    }
};

struct Fixture {
    MockIpBanRepository ban_repo;
    core::ManualClock clock{core::SystemTime{}};

    CheckIpBan make_use_case() {
        return CheckIpBan{ban_repo};
    }
};

}  // namespace

TEST_CASE("CheckIpBan: unbanned IP returns banned = false",
          "[application][moderation][ipban]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(make_ip("192.168.1.1"));

    REQUIRE(r);
    REQUIRE_FALSE(r.value().banned);
    REQUIRE(r.value().reason.empty());
}

TEST_CASE("CheckIpBan: function executes without errors",
          "[application][moderation][ipban]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(make_ip("10.0.0.1"));

    REQUIRE(r);
}

TEST_CASE("CheckIpBan: can check multiple different IPs",
          "[application][moderation][ipban]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r1 = uc.execute(make_ip("192.168.1.1"));
    auto r2 = uc.execute(make_ip("172.16.0.1"));

    REQUIRE(r1);
    REQUIRE(r2);
}

TEST_CASE("CheckIpBan: ipv4 validation",
          "[application][moderation][ipban]") {
    Fixture f;
    auto uc = f.make_use_case();

    // Valid IPv4
    auto r = uc.execute(make_ip("127.0.0.1"));
    REQUIRE(r);
}
