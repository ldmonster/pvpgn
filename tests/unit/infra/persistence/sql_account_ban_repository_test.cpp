// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_account_ban_repository_test.cpp
//
// Verifies the driver-parameterized SqlAccountBanRepository by
// running it over a RECORDING FAKE IDbDriver. This exercises the repository's
// SQL generation, parameter binding and row → domain mapping WITHOUT a live
// database — so it runs in any environment (the SQLite integration test is
// gated on sqlite3.h being available). The same repository runs unchanged over
// the real sqlite/mysql/postgres drivers; this proves its logic in isolation.

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/moderation/ports.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/account_ban_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::AccountId;
using pvpgn::domain::moderation::AccountBan;

namespace {

pvpgn::core::SystemTime epoch_plus(std::int64_t secs) {
    return pvpgn::core::SystemTime{} + std::chrono::seconds{secs};
}

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

}  // namespace

TEST_CASE("SqlAccountBanRepository::add_ban issues the bound upsert",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();
    SqlAccountBanRepository repo{driver};

    AccountBan ban;
    ban.banned_account = AccountId{42};
    ban.banned_by      = AccountId{7};
    ban.reason         = "spamming '; DROP TABLE accounts;--";  // injection-safe via binding
    ban.banned_at      = epoch_plus(1000);
    ban.expires_at     = epoch_plus(5000);

    REQUIRE(repo.add_ban(ban).has_value());

    REQUIRE(driver->calls.size() == 1);
    const auto& call = driver->last();
    CHECK(call.sql.find("INSERT OR REPLACE INTO account_bans") != std::string::npos);
    // The literal text is bound, not interpolated — so it appears only in params.
    CHECK(call.sql.find("DROP TABLE") == std::string::npos);

    REQUIRE(call.params.size() == 5);
    CHECK(as_int(call.params[0]) == 42);    // account_id
    CHECK(as_int(call.params[1]) == 7);     // banned_by
    CHECK(as_str(call.params[2]) == ban.reason);
    CHECK(as_int(call.params[3]) == 1000);  // banned_at epoch seconds
    CHECK(as_int(call.params[4]) == 5000);  // expires_at epoch seconds
}

TEST_CASE("SqlAccountBanRepository::add_ban binds NULL for a permanent ban",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();
    SqlAccountBanRepository repo{driver};

    AccountBan ban;
    ban.banned_account = AccountId{1};
    ban.banned_by      = AccountId{2};
    ban.reason         = "permanent";
    ban.banned_at      = epoch_plus(10);
    ban.expires_at     = std::nullopt;  // permanent

    REQUIRE(repo.add_ban(ban).has_value());

    const auto& call = driver->last();
    REQUIRE(call.params.size() == 5);
    CHECK(is_null_param(call.params[4]));  // expires_at NULL
}

TEST_CASE("SqlAccountBanRepository::find_active_ban maps a row and respects expiry",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();
    SqlAccountBanRepository repo{driver};

    // One stored ban for account 42: banned_by 7, banned_at=1000, expires=5000.
    driver->next_rows.emplace_back(std::vector<Cell>{
        std::int64_t{42}, std::int64_t{7}, std::string{"abuse"},
        std::int64_t{1000}, std::int64_t{5000}});

    SECTION("active when now is before expiry") {
        auto r = repo.find_active_ban(AccountId{42}, epoch_plus(3000));
        REQUIRE(r.has_value());
        REQUIRE(r.value().has_value());
        const AccountBan& ban = *r.value();
        CHECK(ban.banned_account.value() == 42u);
        CHECK(ban.banned_by.value() == 7u);
        CHECK(ban.reason == "abuse");
        CHECK(ban.banned_at == epoch_plus(1000));
        REQUIRE(ban.expires_at.has_value());
        CHECK(*ban.expires_at == epoch_plus(5000));

        // The lookup was parameter-bound on the account id.
        const auto& call = driver->last();
        CHECK(call.sql.find("WHERE account_id = ?") != std::string::npos);
        REQUIRE(call.params.size() == 1);
        CHECK(as_int(call.params[0]) == 42);
    }

    SECTION("not active once now is past expiry") {
        auto r = repo.find_active_ban(AccountId{42}, epoch_plus(9000));
        REQUIRE(r.has_value());
        CHECK_FALSE(r.value().has_value());  // expired → no active ban
    }
}

TEST_CASE("SqlAccountBanRepository::find_active_ban returns empty when no row",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();  // next_rows empty
    SqlAccountBanRepository repo{driver};

    auto r = repo.find_active_ban(AccountId{99}, epoch_plus(0));
    REQUIRE(r.has_value());
    CHECK_FALSE(r.value().has_value());
}

TEST_CASE("SqlAccountBanRepository::find_active_ban treats a permanent ban as active",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();
    SqlAccountBanRepository repo{driver};
    // expires_at is NULL (permanent).
    driver->next_rows.emplace_back(std::vector<Cell>{
        std::int64_t{5}, std::int64_t{6}, std::string{"perm"},
        std::int64_t{100}, nullptr});

    auto r = repo.find_active_ban(AccountId{5}, epoch_plus(1'000'000));
    REQUIRE(r.has_value());
    REQUIRE(r.value().has_value());
    CHECK_FALSE(r.value()->expires_at.has_value());
}

TEST_CASE("SqlAccountBanRepository::remove_ban issues a bound DELETE",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();
    SqlAccountBanRepository repo{driver};

    REQUIRE(repo.remove_ban(AccountId{42}).has_value());

    const auto& call = driver->last();
    CHECK(call.sql.find("DELETE FROM account_bans WHERE account_id = ?")
          != std::string::npos);
    REQUIRE(call.params.size() == 1);
    CHECK(as_int(call.params[0]) == 42);
}

TEST_CASE("SqlAccountBanRepository::for_each maps every row until predicate stops",
          "[infra][persistence][account_ban]") {
    auto driver = make_driver();
    SqlAccountBanRepository repo{driver};

    driver->next_rows.emplace_back(std::vector<Cell>{
        std::int64_t{1}, std::int64_t{10}, std::string{"a"},
        std::int64_t{1}, nullptr});
    driver->next_rows.emplace_back(std::vector<Cell>{
        std::int64_t{2}, std::int64_t{20}, std::string{"b"},
        std::int64_t{2}, std::int64_t{99}});

    std::vector<std::uint32_t> seen;
    repo.for_each([&](const AccountBan& b) {
        seen.push_back(b.banned_account.value());
        return true;  // keep going
    });

    REQUIRE(seen.size() == 2);
    CHECK(seen[0] == 1u);
    CHECK(seen[1] == 2u);

    SECTION("predicate returning false stops iteration") {
        std::vector<std::uint32_t> seen2;
        repo.for_each([&](const AccountBan& b) {
            seen2.push_back(b.banned_account.value());
            return false;  // stop after the first
        });
        CHECK(seen2.size() == 1);
    }
}
