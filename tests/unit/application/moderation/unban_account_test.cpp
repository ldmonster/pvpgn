// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::UnbanAccount`. Exercises the use-case
// for removing an account ban.

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <optional>

#include "application/moderation/unban_account.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "core/clock.hpp"
#include "domain/shared/ids.hpp"

namespace {

using namespace pvpgn;
using application::moderation::UnbanAccount;
using application::moderation::UnbanAccountError;

// ---------------------------------------------------------------------------
// Fakes
// ---------------------------------------------------------------------------

class FakeAccountBanRepository final : public application::ports::IAccountBanRepository {
public:
    bool has_active_ban = true;
    bool remove_fails   = false;

    core::Result<std::optional<application::ports::AccountBan>>
    find_active_ban(domain::AccountId, core::SystemTime) const override {
        if (has_active_ban) {
            return std::optional<application::ports::AccountBan>{
                application::ports::AccountBan{
                    .banned_account = domain::AccountId{42},
                    .banned_by      = domain::AccountId{1},
                    .reason         = "cheating",
                    .banned_at      = core::SystemTime{},
                    .expires_at     = std::nullopt,
                }
            };
        }
        return std::optional<application::ports::AccountBan>{std::nullopt};
    }

    core::Status<> add_ban(const application::ports::AccountBan&) override {
        return core::ok();
    }

    core::Status<> remove_ban(domain::AccountId) override {
        if (remove_fails)
            return core::fail(core::Error{core::StatusCode::Internal, "db error"});
        return core::ok();
    }

    void for_each(std::function<bool(const application::ports::AccountBan&)>) const override {}
};

class FakeEventBus final : public application::ports::IEventBus {
public:
    void publish(const domain::events::DomainEvent&) override {}
    application::ports::SubscriptionId subscribe(application::ports::EventHandler) override { return 0; }
    void unsubscribe(application::ports::SubscriptionId) override {}
};

struct Fixture {
    std::shared_ptr<FakeAccountBanRepository> bans      = std::make_shared<FakeAccountBanRepository>();
    std::shared_ptr<FakeEventBus>             event_bus = std::make_shared<FakeEventBus>();

    UnbanAccount make_use_case() {
        return UnbanAccount{bans, event_bus};
    }
};

}  // namespace

TEST_CASE("UnbanAccount: happy path removes an active ban",
          "[application][moderation][unban_account]") {
    Fixture f;
    auto uc = f.make_use_case();

    auto result = uc.execute(domain::AccountId{42}, domain::AccountId{1});
    REQUIRE(result);
}

TEST_CASE("UnbanAccount: returns NotBanned when account has no active ban",
          "[application][moderation][unban_account]") {
    Fixture f;
    f.bans->has_active_ban = false;
    auto uc = f.make_use_case();

    auto result = uc.execute(domain::AccountId{42}, domain::AccountId{1});
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == UnbanAccountError::NotBanned);
}

TEST_CASE("UnbanAccount: returns PersistenceFailed when remove fails",
          "[application][moderation][unban_account]") {
    Fixture f;
    f.bans->remove_fails = true;
    auto uc = f.make_use_case();

    auto result = uc.execute(domain::AccountId{42}, domain::AccountId{1});
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == UnbanAccountError::PersistenceFailed);
}
