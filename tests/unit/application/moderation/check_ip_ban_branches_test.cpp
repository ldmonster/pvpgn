// SPDX-License-Identifier: GPL-2.0-or-later
//
// Branch coverage for `application::moderation::CheckIpBan`.
// The existing check_ip_ban_test.cpp only exercises the "not banned"
// path with a stub repository. These cases drive the remaining branches:
//   - banned IP whose matching entry supplies reason + expiry,
//   - banned IP whose entry is not found via for_each (default reason),
//   - repository error propagation from is_banned().

#include <chrono>
#include <functional>

#include <catch2/catch_test_macros.hpp>

#include "application/moderation/check_ip_ban.hpp"
#include "domain/moderation/ports.hpp"
#include "core/clock.hpp"
#include "core/error.hpp"
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

// Configurable IP ban repository. `is_banned` and `for_each_entry` are
// driven by std::function hooks so each test can shape one branch.
class FakeIpBanRepository : public domain::moderation::IIpBanRepository {
public:
    std::function<core::Result<bool>(const domain::IpAddress&)> on_is_banned =
        [](const domain::IpAddress&) { return core::Result<bool>{false}; };

    std::vector<domain::moderation::IpBanEntry> entries;

    core::Result<bool> is_banned(const domain::IpAddress& ip) const override {
        return on_is_banned(ip);
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
        std::function<bool(const domain::moderation::IpBanEntry&)> pred)
        const override {
        for (const auto& e : entries) {
            if (!pred(e)) return;
        }
    }

    core::Result<domain::moderation::IpBanList> load_banlist() const override {
        return domain::moderation::IpBanList{};
    }

    core::Status<> save_banlist(const domain::moderation::IpBanList&) override {
        return core::ok();
    }
};

const auto t0 = std::chrono::system_clock::time_point{};

domain::moderation::IpBanEntry entry(std::string_view ip,
                                     std::string reason,
                                     std::optional<core::SystemTime> exp) {
    return domain::moderation::IpBanEntry{
        domain::IpAddress::parse(ip).value(), std::move(reason),
        domain::AccountId{42}, t0, exp};
}

}  // namespace

TEST_CASE("CheckIpBan: banned IP returns reason and expiry from matching entry",
          "[application][moderation][ipban]") {
    FakeIpBanRepository repo;
    repo.on_is_banned = [](const domain::IpAddress&) {
        return core::Result<bool>{true};
    };
    const auto exp = t0 + std::chrono::hours(3);
    repo.entries.push_back(entry("203.0.113.7", "abuse", exp));

    CheckIpBan uc{repo};
    auto r = uc.execute(make_ip("203.0.113.7"));

    REQUIRE(r);
    REQUIRE(r.value().banned);
    REQUIRE(r.value().reason == "abuse");
    REQUIRE(r.value().expires_at.has_value());
    REQUIRE(*r.value().expires_at == exp);
}

TEST_CASE("CheckIpBan: for_each stops at the matching entry, skipping later ones",
          "[application][moderation][ipban]") {
    FakeIpBanRepository repo;
    repo.on_is_banned = [](const domain::IpAddress&) {
        return core::Result<bool>{true};
    };
    repo.entries.push_back(entry("203.0.113.1", "first", std::nullopt));
    repo.entries.push_back(entry("203.0.113.7", "target", std::nullopt));
    repo.entries.push_back(entry("203.0.113.9", "later", std::nullopt));

    CheckIpBan uc{repo};
    auto r = uc.execute(make_ip("203.0.113.7"));

    REQUIRE(r);
    REQUIRE(r.value().banned);
    REQUIRE(r.value().reason == "target");
    REQUIRE_FALSE(r.value().expires_at.has_value());
}

TEST_CASE("CheckIpBan: banned IP with no matching entry keeps the default reason",
          "[application][moderation][ipban]") {
    // is_banned() is true (e.g. matched by a CIDR range) but for_each_entry
    // yields no exact host entry — the use-case keeps reason = "Banned".
    FakeIpBanRepository repo;
    repo.on_is_banned = [](const domain::IpAddress&) {
        return core::Result<bool>{true};
    };
    // entries left empty.

    CheckIpBan uc{repo};
    auto r = uc.execute(make_ip("198.51.100.5"));

    REQUIRE(r);
    REQUIRE(r.value().banned);
    REQUIRE(r.value().reason == "Banned");
    REQUIRE_FALSE(r.value().expires_at.has_value());
}

TEST_CASE("CheckIpBan: repository error from is_banned is propagated",
          "[application][moderation][ipban]") {
    FakeIpBanRepository repo;
    repo.on_is_banned = [](const domain::IpAddress&) {
        return core::fail(core::Error{
            core::StatusCode::Internal, "backend down"});
    };

    CheckIpBan uc{repo};
    auto r = uc.execute(make_ip("203.0.113.7"));

    REQUIRE_FALSE(r);
    REQUIRE(r.error().code() == core::StatusCode::Internal);
}
