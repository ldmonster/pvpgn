// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::moderation::IssueWarning`. Exercises the use-case
// for recording a formal warning against an account.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "application/moderation/issue_warning.hpp"
#include "domain/identity/ports.hpp"
#include "domain/moderation/ports.hpp"
#include "domain/identity/account.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/locale.hpp"
#include "domain/shared/user_name.hpp"

namespace {

using namespace pvpgn;
using application::moderation::IssueWarning;
using application::moderation::IssueWarningCommand;
using application::moderation::IssueWarningError;

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

class FakeAccountRepository final : public domain::identity::IAccountRepository {
public:
    bool account_exists = true;

    core::Result<domain::identity::Account, core::Error>
    find_by_name(std::string_view name) override {
        if (!account_exists)
            return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
        return make_account(domain::AccountId{42}, name.empty() ? "target" : name);
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

class FakeAuditLog final : public domain::moderation::IAuditLog {
public:
    std::vector<domain::moderation::AuditEntry> recorded;

    void record(const domain::moderation::AuditEntry& entry) override {
        recorded.push_back(entry);
    }

    std::vector<domain::moderation::AuditEntry>
    recent(std::size_t count) const override {
        if (recorded.size() <= count) return recorded;
        return {recorded.end() - static_cast<std::ptrdiff_t>(count), recorded.end()};
    }
};

struct Fixture {
    std::shared_ptr<FakeAccountRepository> accounts = std::make_shared<FakeAccountRepository>();
    std::shared_ptr<FakeAuditLog>          audit    = std::make_shared<FakeAuditLog>();

    IssueWarning make_use_case() {
        return IssueWarning{accounts, audit};
    }
};

}  // namespace

TEST_CASE("IssueWarning: happy path issues a warning and records audit entry",
          "[application][moderation][issue_warning]") {
    Fixture f;
    auto uc = f.make_use_case();

    IssueWarningCommand cmd{
        .admin_id            = domain::AccountId{1},
        .admin_name          = "admin",
        .target_account_name = "badplayer",
        .reason              = "toxic behaviour",
        .warning_level       = 1,
    };

    auto result = uc.execute(cmd);
    REQUIRE(result);
    REQUIRE(f.audit->recorded.size() == 1);
    REQUIRE(f.audit->recorded[0].subject == "badplayer");
    REQUIRE(f.audit->recorded[0].details == "toxic behaviour");
}

TEST_CASE("IssueWarning: returns TargetNotFound when account does not exist",
          "[application][moderation][issue_warning]") {
    Fixture f;
    f.accounts->account_exists = false;
    auto uc = f.make_use_case();

    IssueWarningCommand cmd{
        .admin_id            = domain::AccountId{1},
        .admin_name          = "admin",
        .target_account_name = "ghost",
        .reason              = "cheating",
        .warning_level       = 2,
    };

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == IssueWarningError::TargetNotFound);
    REQUIRE(f.audit->recorded.empty());
}

TEST_CASE("IssueWarning: returns InvalidWarningLevel when level is 0",
          "[application][moderation][issue_warning]") {
    Fixture f;
    auto uc = f.make_use_case();

    IssueWarningCommand cmd{
        .admin_id            = domain::AccountId{1},
        .admin_name          = "admin",
        .target_account_name = "badplayer",
        .reason              = "cheating",
        .warning_level       = 0,
    };

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == IssueWarningError::InvalidWarningLevel);
}

TEST_CASE("IssueWarning: returns InvalidWarningLevel when level exceeds 3",
          "[application][moderation][issue_warning]") {
    Fixture f;
    auto uc = f.make_use_case();

    IssueWarningCommand cmd{
        .admin_id            = domain::AccountId{1},
        .admin_name          = "admin",
        .target_account_name = "badplayer",
        .reason              = "cheating",
        .warning_level       = 4,
    };

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == IssueWarningError::InvalidWarningLevel);
}

TEST_CASE("IssueWarning: returns InvalidReason when reason is empty",
          "[application][moderation][issue_warning]") {
    Fixture f;
    auto uc = f.make_use_case();

    IssueWarningCommand cmd{
        .admin_id            = domain::AccountId{1},
        .admin_name          = "admin",
        .target_account_name = "badplayer",
        .reason              = "",
        .warning_level       = 1,
    };

    auto result = uc.execute(cmd);
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == IssueWarningError::InvalidReason);
}
