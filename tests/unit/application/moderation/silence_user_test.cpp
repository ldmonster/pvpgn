// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::SilenceUser`. Exercises the use-case
// for muting a user's chat temporarily.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "application/moderation/silence_user.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/event_bus.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace {

using namespace pvpgn;
using application::moderation::SilenceUser;
using application::moderation::SilenceUserError;
using application::moderation::SilenceUserRequest;

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

// Migrated to the current IAccountRepository port (domain::AccountId /
// domain::UserName, const finders, forEach/size).
class FakeAccountRepository final : public domain::identity::IAccountRepository {
public:
    bool account_exists = true;
    bool save_fails     = false;

    core::Result<domain::identity::Account>
    find_by_id(domain::AccountId id) const override {
        if (!account_exists)
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        return make_account(id, "target");
    }

    core::Result<domain::identity::Account>
    find_by_name(const domain::UserName& name) const override {
        if (!account_exists)
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        return make_account(domain::AccountId{42}, name.display());
    }

    core::Status<> save(const domain::identity::Account&) override {
        if (save_fails)
            return core::fail(core::Error{core::StatusCode::Internal, "db error"});
        return core::ok();
    }

    core::Status<> remove(domain::AccountId) override { return core::ok(); }

    void forEach(std::function<bool(const domain::identity::Account&)>) const override {}

    std::size_t size() const noexcept override { return account_exists ? 1u : 0u; }
};

class FakeEventBus final : public application::ports::IEventBus {
public:
    void publish(const domain::events::DomainEvent&) override {}
    application::ports::SubscriptionId subscribe(application::ports::EventHandler) override { return 0; }
    void unsubscribe(application::ports::SubscriptionId) override {}
};

struct Fixture {
    std::shared_ptr<FakeAccountRepository> accounts  = std::make_shared<FakeAccountRepository>();
    std::shared_ptr<FakeEventBus>          event_bus = std::make_shared<FakeEventBus>();

    SilenceUser make_use_case() {
        return SilenceUser{accounts, event_bus};
    }
};

}  // namespace

TEST_CASE("SilenceUser: happy path silences an existing account",
          "[application][moderation][silence_user]") {
    Fixture f;
    auto uc = f.make_use_case();

    SilenceUserRequest req{
        .target      = domain::AccountId{42},
        .silenced_by = domain::AccountId{1},
        .duration    = std::chrono::seconds{3600},
        .reason      = "spamming",
    };

    auto result = uc.execute(req);
    REQUIRE(result);
}

TEST_CASE("SilenceUser: returns TargetNotFound when account does not exist",
          "[application][moderation][silence_user]") {
    Fixture f;
    f.accounts->account_exists = false;
    auto uc = f.make_use_case();

    SilenceUserRequest req{
        .target      = domain::AccountId{99},
        .silenced_by = domain::AccountId{1},
        .duration    = std::chrono::seconds{3600},
        .reason      = "spamming",
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == SilenceUserError::TargetNotFound);
}

TEST_CASE("SilenceUser: returns InvalidDuration when duration is zero",
          "[application][moderation][silence_user]") {
    Fixture f;
    auto uc = f.make_use_case();

    SilenceUserRequest req{
        .target      = domain::AccountId{42},
        .silenced_by = domain::AccountId{1},
        .duration    = std::chrono::seconds{0},
        .reason      = "spamming",
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == SilenceUserError::InvalidDuration);
}

TEST_CASE("SilenceUser: returns InvalidReason when reason is empty",
          "[application][moderation][silence_user]") {
    Fixture f;
    auto uc = f.make_use_case();

    SilenceUserRequest req{
        .target      = domain::AccountId{42},
        .silenced_by = domain::AccountId{1},
        .duration    = std::chrono::seconds{3600},
        .reason      = "",
    };

    auto result = uc.execute(req);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == SilenceUserError::InvalidReason);
}
