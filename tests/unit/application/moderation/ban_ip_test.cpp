// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::BanIp`. Exercises the use-case
// for banning an IP address or CIDR range.

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <optional>

#include "application/moderation/ban_ip.hpp"
#include "application/ports/event_bus.hpp"
#include "application/ports/ip_ban_repository.hpp"
#include "core/clock.hpp"
#include "domain/moderation/ip_ban_list.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"

namespace {

using namespace pvpgn;
using application::moderation::BanIp;
using application::moderation::BanIpError;
using application::moderation::BanIpRequest;

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

class FakeIpBanRepository final : public application::ports::IIpBanRepository {
public:
    bool already_banned = false;
    bool save_fails     = false;

    core::Result<bool> is_banned(const domain::IpAddress&) const override {
        return already_banned;
    }

    core::Status<> add_ban(domain::moderation::IpBanEntry) override {
        if (save_fails)
            return core::fail(core::Error{core::StatusCode::Internal, "db error"});
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
        std::function<bool(const domain::moderation::IpBanEntry&)>) const override {}

    core::Result<domain::moderation::IpBanList> load_banlist() const override {
        return domain::moderation::IpBanList{};
    }

    core::Status<> save_banlist(const domain::moderation::IpBanList&) override {
        return core::ok();
    }
};

class FakeEventBus final : public application::ports::IEventBus {
public:
    void publish(const domain::events::DomainEvent&) override {}
    application::ports::SubscriptionId subscribe(application::ports::EventHandler) override { return 0; }
    void unsubscribe(application::ports::SubscriptionId) override {}
};

struct Fixture {
    std::shared_ptr<FakeIpBanRepository> bans      = std::make_shared<FakeIpBanRepository>();
    std::shared_ptr<FakeEventBus>        event_bus = std::make_shared<FakeEventBus>();

    BanIp make_use_case() {
        return BanIp{bans, event_bus};
    }
};

}  // namespace

TEST_CASE("BanIp: happy path bans an IP address",
          "[application][moderation][ban_ip]") {
    Fixture f;
    auto uc = f.make_use_case();

    BanIpRequest req{
        .target       = make_ip("192.168.1.100"),
        .banned_by    = domain::AccountId{1},
        .reason       = "hacking",
        .is_cidr_range = false,
        .expires_at   = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE(result);
}

TEST_CASE("BanIp: returns InvalidReason when reason is empty",
          "[application][moderation][ban_ip]") {
    Fixture f;
    auto uc = f.make_use_case();

    BanIpRequest req{
        .target       = make_ip("10.0.0.1"),
        .banned_by    = domain::AccountId{1},
        .reason       = "",
        .is_cidr_range = false,
        .expires_at   = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == BanIpError::InvalidReason);
}

TEST_CASE("BanIp: returns AlreadyBanned when IP is already banned",
          "[application][moderation][ban_ip]") {
    Fixture f;
    f.bans->already_banned = true;
    auto uc = f.make_use_case();

    BanIpRequest req{
        .target       = make_ip("192.168.1.100"),
        .banned_by    = domain::AccountId{1},
        .reason       = "hacking",
        .is_cidr_range = false,
        .expires_at   = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == BanIpError::AlreadyBanned);
}
