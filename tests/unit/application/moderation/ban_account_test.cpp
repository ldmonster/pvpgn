// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::BanAccount`. Exercises the use-case
// for banning an account from logging in.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "application/moderation/ban_account.hpp"
#include "application/ports/account_ban_repository.hpp"
#include "application/ports/account_repository.hpp"
#include "application/ports/event_bus.hpp"
#include "core/clock.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace {

using namespace pvpgn;
using application::moderation::BanAccount;
using application::moderation::BanAccountError;
using application::moderation::BanAccountRequest;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

domain::identity::Account make_account(domain::AccountId id,
                                        std::string_view name) {
    auto uname = domain::UserName::parse(name);
    REQUIRE(uname);
    auto locale = domain::Locale::parse_or_default("enUS");
    return domain::identity::Account::rehydrate(
        id, uname.value(), domain::BNHash{}, locale,
        domain::identity::CommandGroupMask{}, std::nullopt, false);
}

// ---------------------------------------------------------------------------
// Fakes
// ---------------------------------------------------------------------------

class FakeAccountRepository final : public application::ports::IAccountRepository {
public:
    bool account_exists = true;

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override {
        if (!account_exists)
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        return make_account(domain::AccountId{1}, name.empty() ? "target" : name);
    }

    core::Result<domain::identity::Account, core::Error>
    find_by_id(uint32_t id) override {
        if (!account_exists)
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        return make_account(domain::AccountId{id}, "target");
    }

    core::Result<void, core::Error>
    save(const domain::identity::Account&) override { return {}; }

    core::Result<void, core::Error>
    remove(std::string_view) override { return {}; }

    core::Result<bool, core::Error>
    exists(std::string_view) override { return account_exists; }

    core::Result<std::vector<domain::identity::Account>, core::Error>
    list_online() override { return std::vector<domain::identity::Account>{}; }

    core::Result<uint32_t, core::Error>
    count() override { return 0u; }
};

class FakeAccountBanRepository final : public application::ports::IAccountBanRepository {
public:
    bool has_active_ban = false;
    bool save_fails     = false;

    core::Result<std::optional<application::ports::AccountBan>>
    find_active_ban(domain::AccountId, core::SystemTime) const override {
        if (has_active_ban) {
            return std::optional<application::ports::AccountBan>{
                application::ports::AccountBan{
                    .banned_account = domain::AccountId{1},
                    .banned_by      = domain::AccountId{2},
                    .reason         = "test",
                    .banned_at      = core::SystemTime{},
                    .expires_at     = std::nullopt,
                }
            };
        }
        return std::optional<application::ports::AccountBan>{std::nullopt};
    }

    core::Status<> add_ban(const application::ports::AccountBan&) override {
        if (save_fails)
            return core::fail(core::Error{core::StatusCode::Internal, "db error"});
        return core::ok();
    }

    core::Status<> remove_ban(domain::AccountId) override { return core::ok(); }

    void for_each(std::function<bool(const application::ports::AccountBan&)>) const override {}
};

class FakeEventBus final : public application::ports::IEventBus {
public:
    void publish(const domain::events::DomainEvent&) override {}
    application::ports::SubscriptionId subscribe(application::ports::EventHandler) override { return 0; }
    void unsubscribe(application::ports::SubscriptionId) override {}
};

struct Fixture {
    std::shared_ptr<FakeAccountRepository>    accounts  = std::make_shared<FakeAccountRepository>();
    std::shared_ptr<FakeAccountBanRepository> bans      = std::make_shared<FakeAccountBanRepository>();
    std::shared_ptr<FakeEventBus>             event_bus = std::make_shared<FakeEventBus>();

    BanAccount make_use_case() {
        return BanAccount{bans, accounts, event_bus};
    }
};

}  // namespace

TEST_CASE("BanAccount: happy path bans an existing account",
          "[application][moderation][ban_account]") {
    Fixture f;
    auto uc = f.make_use_case();

    BanAccountRequest req{
        .target     = domain::AccountId{42},
        .banned_by  = domain::AccountId{1},
        .reason     = "cheating",
        .expires_at = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE(result);
}

TEST_CASE("BanAccount: returns TargetNotFound when account does not exist",
          "[application][moderation][ban_account]") {
    Fixture f;
    f.accounts->account_exists = false;
    auto uc = f.make_use_case();

    BanAccountRequest req{
        .target     = domain::AccountId{99},
        .banned_by  = domain::AccountId{1},
        .reason     = "cheating",
        .expires_at = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == BanAccountError::TargetNotFound);
}

TEST_CASE("BanAccount: returns InvalidReason when reason is empty",
          "[application][moderation][ban_account]") {
    Fixture f;
    auto uc = f.make_use_case();

    BanAccountRequest req{
        .target     = domain::AccountId{42},
        .banned_by  = domain::AccountId{1},
        .reason     = "",
        .expires_at = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == BanAccountError::InvalidReason);
}

TEST_CASE("BanAccount: returns AlreadyBanned when account is already banned",
          "[application][moderation][ban_account]") {
    Fixture f;
    f.bans->has_active_ban = true;
    auto uc = f.make_use_case();

    BanAccountRequest req{
        .target     = domain::AccountId{42},
        .banned_by  = domain::AccountId{1},
        .reason     = "cheating",
        .expires_at = std::nullopt,
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == BanAccountError::AlreadyBanned);
}
