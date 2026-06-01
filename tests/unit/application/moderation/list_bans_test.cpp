// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::ListBans`. Exercises the use-case
// for enumerating active account and IP bans.

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <optional>
#include <vector>

#include "application/moderation/list_bans.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "core/clock.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace {

using namespace pvpgn;
using application::moderation::BanRecord;
using application::moderation::BanType;
using application::moderation::ListBans;
using application::moderation::ListBansQuery;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

domain::IpAddress make_ip(std::string_view s) {
    auto r = domain::IpAddress::parse(s);
    REQUIRE(r);
    return r.value();
}

// ---------------------------------------------------------------------------
// Fakes
// ---------------------------------------------------------------------------

class FakeAccountBanRepository final : public application::ports::IAccountBanRepository {
public:
    std::vector<application::ports::AccountBan> bans;

    core::Result<std::optional<application::ports::AccountBan>>
    find_active_ban(domain::AccountId, core::SystemTime) const override {
        return std::optional<application::ports::AccountBan>{std::nullopt};
    }

    core::Status<> add_ban(const application::ports::AccountBan&) override {
        return core::ok();
    }

    core::Status<> remove_ban(domain::AccountId) override { return core::ok(); }

    void for_each(std::function<bool(const application::ports::AccountBan&)> pred) const override {
        for (const auto& b : bans) {
            if (!pred(b)) break;
        }
    }
};

class FakeIpBanRepository final : public application::ports::IIpBanRepository {
public:
    std::vector<domain::moderation::IpBanEntry> entries;

    core::Result<bool> is_banned(const domain::IpAddress&) const override {
        return false;
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

    core::Status<> remove_ban(const domain::IpAddress&) override { return core::ok(); }

    core::Status<> remove_range_ban(domain::IpAddress, std::uint8_t) override {
        return core::ok();
    }

    void for_each_entry(
        std::function<bool(const domain::moderation::IpBanEntry&)> pred) const override {
        for (const auto& e : entries) {
            if (!pred(e)) break;
        }
    }

    core::Result<domain::moderation::IpBanList> load_banlist() const override {
        return domain::moderation::IpBanList{};
    }

    core::Status<> save_banlist(const domain::moderation::IpBanList&) override {
        return core::ok();
    }
};

struct Fixture {
    std::shared_ptr<FakeAccountBanRepository> account_bans = std::make_shared<FakeAccountBanRepository>();
    std::shared_ptr<FakeIpBanRepository>      ip_bans      = std::make_shared<FakeIpBanRepository>();

    ListBans make_use_case() {
        return ListBans{account_bans, ip_bans};
    }
};

}  // namespace

TEST_CASE("ListBans: returns empty list when no bans exist",
          "[application][moderation][list_bans]") {
    Fixture f;
    auto uc = f.make_use_case();

    auto result = uc.execute(ListBansQuery{});
    REQUIRE(result);
    REQUIRE(result.value().empty());
}

TEST_CASE("ListBans: returns account bans when filter is Account",
          "[application][moderation][list_bans]") {
    Fixture f;
    f.account_bans->bans.push_back(application::ports::AccountBan{
        .banned_account = domain::AccountId{10},
        .banned_by      = domain::AccountId{1},
        .reason         = "cheating",
        .banned_at      = core::SystemTime{},
        .expires_at     = std::nullopt,
    });
    f.ip_bans->entries.push_back(domain::moderation::IpBanEntry{
        .ip        = make_ip("1.2.3.4"),
        .reason    = "hacking",
        .issuer    = domain::AccountId{1},
        .issued_at = core::SystemTime{},
        .expires_at = std::nullopt,
    });

    auto uc = f.make_use_case();
    auto result = uc.execute(ListBansQuery{.filter_type = BanType::Account});
    REQUIRE(result);
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].type == BanType::Account);
    REQUIRE(result.value()[0].reason == "cheating");
}

TEST_CASE("ListBans: returns IP bans when filter is Ip",
          "[application][moderation][list_bans]") {
    Fixture f;
    f.account_bans->bans.push_back(application::ports::AccountBan{
        .banned_account = domain::AccountId{10},
        .banned_by      = domain::AccountId{1},
        .reason         = "cheating",
        .banned_at      = core::SystemTime{},
        .expires_at     = std::nullopt,
    });
    f.ip_bans->entries.push_back(domain::moderation::IpBanEntry{
        .ip        = make_ip("1.2.3.4"),
        .reason    = "hacking",
        .issuer    = domain::AccountId{1},
        .issued_at = core::SystemTime{},
        .expires_at = std::nullopt,
    });

    auto uc = f.make_use_case();
    auto result = uc.execute(ListBansQuery{.filter_type = BanType::Ip});
    REQUIRE(result);
    REQUIRE(result.value().size() == 1);
    REQUIRE(result.value()[0].type == BanType::Ip);
    REQUIRE(result.value()[0].reason == "hacking");
}

TEST_CASE("ListBans: returns all bans when filter is nullopt",
          "[application][moderation][list_bans]") {
    Fixture f;
    f.account_bans->bans.push_back(application::ports::AccountBan{
        .banned_account = domain::AccountId{10},
        .banned_by      = domain::AccountId{1},
        .reason         = "cheating",
        .banned_at      = core::SystemTime{},
        .expires_at     = std::nullopt,
    });
    f.ip_bans->entries.push_back(domain::moderation::IpBanEntry{
        .ip        = make_ip("1.2.3.4"),
        .reason    = "hacking",
        .issuer    = domain::AccountId{1},
        .issued_at = core::SystemTime{},
        .expires_at = std::nullopt,
    });

    auto uc = f.make_use_case();
    auto result = uc.execute(ListBansQuery{.filter_type = std::nullopt});
    REQUIRE(result);
    REQUIRE(result.value().size() == 2);
}

TEST_CASE("ListBans: respects max_results limit",
          "[application][moderation][list_bans]") {
    Fixture f;
    // Add 3 account bans
    for (uint32_t i = 1; i <= 3; ++i) {
        f.account_bans->bans.push_back(application::ports::AccountBan{
            .banned_account = domain::AccountId{i},
            .banned_by      = domain::AccountId{1},
            .reason         = "spam",
            .banned_at      = core::SystemTime{},
            .expires_at     = std::nullopt,
        });
    }

    auto uc = f.make_use_case();
    auto result = uc.execute(ListBansQuery{.filter_type = std::nullopt, .max_results = 2});
    REQUIRE(result);
    REQUIRE(result.value().size() == 2);
}
