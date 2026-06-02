// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_ladder_repository_test.cpp -- Plan 07.
//
// Verifies SqlLadderRepository over the recording fake IDbDriver (no sqlite).
// Pins the bound upsert, the two-query rank computation (lookup rating, then
// count strictly-higher), the NotFound path, and the top-N ordered read.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/ladder/ladder.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/ladder_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::AccountId;
using pvpgn::domain::ladder::LadderEntry;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

// ladder row: account_id, rating, wins, losses, disconnects
FakeRow ladder_row(std::int64_t acct, std::int64_t rating, std::int64_t wins,
                   std::int64_t losses, std::int64_t disc) {
    return FakeRow{std::vector<Cell>{acct, rating, wins, losses, disc}};
}

}  // namespace

TEST_CASE("SqlLadderRepository::save_entry issues the bound upsert",
          "[infra][persistence][ladder]") {
    auto driver = make_driver();
    SqlLadderRepository repo{driver};

    LadderEntry e;
    e.account = AccountId{42};
    e.rating = 1700;
    e.wins = 10;
    e.losses = 3;
    e.disconnects = 1;

    REQUIRE(repo.save_entry(e).has_value());
    const auto& call = driver->last();
    CHECK(call.sql.find("INSERT OR REPLACE INTO ladder") != std::string::npos);
    REQUIRE(call.params.size() == 5);
    CHECK(as_int(call.params[0]) == 42);
    CHECK(as_int(call.params[1]) == 1700);
    CHECK(as_int(call.params[2]) == 10);
    CHECK(as_int(call.params[3]) == 3);
    CHECK(as_int(call.params[4]) == 1);
}

TEST_CASE("SqlLadderRepository::get_rank is 1 + accounts rated higher",
          "[infra][persistence][ladder]") {
    auto driver = make_driver();
    SqlLadderRepository repo{driver};

    // Query 1: the account's rating (1500). Query 2: COUNT of higher = 2.
    driver->push_result_set({FakeRow{std::vector<Cell>{std::int64_t{1500}}}});
    driver->push_result_set({FakeRow{std::vector<Cell>{std::int64_t{2}}}});

    auto r = repo.get_rank(AccountId{42});
    REQUIRE(r.has_value());
    CHECK(r.value() == 3u);  // 2 higher → rank 3

    REQUIRE(driver->calls.size() == 2);
    CHECK(driver->calls[0].sql.find("SELECT rating FROM ladder WHERE account_id = ?")
          != std::string::npos);
    CHECK(as_int(driver->calls[0].params.at(0)) == 42);
    CHECK(driver->calls[1].sql.find("COUNT(*) FROM ladder WHERE rating > ?")
          != std::string::npos);
    CHECK(as_int(driver->calls[1].params.at(0)) == 1500);
}

TEST_CASE("SqlLadderRepository::get_rank is 1 for the top account",
          "[infra][persistence][ladder]") {
    auto driver = make_driver();
    SqlLadderRepository repo{driver};
    driver->push_result_set({FakeRow{std::vector<Cell>{std::int64_t{2000}}}});
    driver->push_result_set({FakeRow{std::vector<Cell>{std::int64_t{0}}}});

    auto r = repo.get_rank(AccountId{1});
    REQUIRE(r.has_value());
    CHECK(r.value() == 1u);
}

TEST_CASE("SqlLadderRepository::get_rank is NotFound when off the ladder",
          "[infra][persistence][ladder]") {
    auto driver = make_driver();
    SqlLadderRepository repo{driver};
    driver->push_result_set({});  // no rating row

    auto r = repo.get_rank(AccountId{999});
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == pvpgn::core::StatusCode::NotFound);
    // The second (COUNT) query must not run if the account is absent.
    CHECK(driver->calls.size() == 1);
}

TEST_CASE("SqlLadderRepository::get_top_n reads ordered, limited rows",
          "[infra][persistence][ladder]") {
    auto driver = make_driver();
    SqlLadderRepository repo{driver};
    driver->push_result_set({
        ladder_row(1, 2000, 20, 1, 0),
        ladder_row(2, 1800, 15, 5, 2)});

    auto r = repo.get_top_n(2);
    REQUIRE(r.has_value());
    REQUIRE(r.value().size() == 2);
    CHECK(r.value()[0].account.value() == 1u);
    CHECK(r.value()[0].rating == 2000);
    CHECK(r.value()[1].account.value() == 2u);
    CHECK(r.value()[1].disconnects == 2u);

    const auto& call = driver->last();
    CHECK(call.sql.find("ORDER BY rating DESC LIMIT ?") != std::string::npos);
    CHECK(as_int(call.params.at(0)) == 2);
}
