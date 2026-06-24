// SPDX-License-Identifier: GPL-2.0-or-later
//
// tests/unit/infra/persistence/sql_game_repository_test.cpp
//
// Verifies SqlGameRepository over the recording fake IDbDriver (no sqlite).
// Game is a parent row + ordered player list, so find_* / list_active issue
// a header query then per-game player queries (exercised via the per-query
// result-set queue). Also pins the transactional save and tag-scoped remove,
// and that Game::rehydrate restores the persisted state.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/persistence/game_repository.hpp"

#include "recording_fake_driver.hpp"

using namespace pvpgn::infra::persistence;
using namespace pvpgn::test::persistence;
using pvpgn::domain::AccountId;
using pvpgn::domain::ClientTag;
using pvpgn::domain::GameId;
using pvpgn::domain::gameplay::Game;
using pvpgn::domain::gameplay::GameDescriptor;
using pvpgn::domain::gameplay::GameState;

namespace {

std::shared_ptr<RecordingFakeDriver> make_driver() {
    return std::make_shared<RecordingFakeDriver>();
}

// games row: id, host, client_tag, name, map, max_players, state
FakeRow game_row(std::int64_t id, std::int64_t host, std::string client,
                 std::string name, std::string map, std::int64_t max_players,
                 std::int64_t state) {
    return FakeRow{std::vector<Cell>{id, host, std::move(client),
                                     std::move(name), std::move(map),
                                     max_players, state}};
}

FakeRow player_row(std::int64_t account) {
    return FakeRow{std::vector<Cell>{account}};
}

}  // namespace

TEST_CASE("SqlGameRepository::find_by_id loads the game, players and state",
          "[infra][persistence][game]") {
    auto driver = make_driver();
    SqlGameRepository repo{driver};

    driver->push_result_set({game_row(
        5, 1, "WAR3", "EpicGame", "Azeroth.w3m", 8,
        static_cast<std::int64_t>(GameState::InProgress))});
    driver->push_result_set({player_row(1), player_row(2), player_row(3)});

    auto r = repo.find_by_id(5);
    REQUIRE(r.has_value());
    auto game = r.value();
    REQUIRE(game != nullptr);
    CHECK(game->id().value() == 5u);
    CHECK(game->host().value() == 1u);
    CHECK(game->client().text() == "WAR3");
    CHECK(game->descriptor().name == "EpicGame");
    CHECK(game->descriptor().map == "Azeroth.w3m");
    CHECK(game->descriptor().max_players == 8);
    CHECK(game->state() == GameState::InProgress);
    REQUIRE(game->players().size() == 3);
    CHECK(game->players()[0].value() == 1u);
    CHECK(game->players()[2].value() == 3u);

    REQUIRE(driver->calls.size() == 2);
    CHECK(driver->calls[0].sql.find("FROM games WHERE id = ?") != std::string::npos);
    CHECK(driver->calls[1].sql.find("FROM game_players WHERE game_id = ?")
          != std::string::npos);
    CHECK(driver->calls[1].sql.find("ORDER BY position") != std::string::npos);
}

TEST_CASE("SqlGameRepository::find_by_id is NotFound when the game is absent",
          "[infra][persistence][game]") {
    auto driver = make_driver();
    SqlGameRepository repo{driver};
    driver->push_result_set({});  // no game row

    auto r = repo.find_by_id(404);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == pvpgn::core::StatusCode::NotFound);
}

TEST_CASE("SqlGameRepository::find rejects an invalid stored client_tag",
          "[infra][persistence][game]") {
    auto driver = make_driver();
    SqlGameRepository repo{driver};
    driver->push_result_set({game_row(5, 1, "TOOLONG", "G", "m", 8, 0)});

    auto r = repo.find_by_id(5);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().code() == pvpgn::core::StatusCode::Internal);
}

TEST_CASE("SqlGameRepository::save upserts the game and replaces players atomically",
          "[infra][persistence][game]") {
    auto driver = make_driver();
    SqlGameRepository repo{driver};

    Game game = Game::rehydrate(
        GameId{5}, AccountId{1}, ClientTag::parse("WAR3").value(),
        GameDescriptor{"EpicGame", "Azeroth.w3m", 8}, GameState::InProgress,
        {AccountId{1}, AccountId{2}});

    REQUIRE(repo.save(game).has_value());

    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    CHECK(driver->rollback_count == 0);

    REQUIRE(driver->calls.size() == 4);  // upsert game, delete players, 2 inserts
    CHECK(driver->calls[0].sql.find("INSERT OR REPLACE INTO games")
          != std::string::npos);
    CHECK(as_int(driver->calls[0].params.at(0)) == 5);     // id
    CHECK(as_int(driver->calls[0].params.at(1)) == 1);     // host
    CHECK(as_str(driver->calls[0].params.at(2)) == "WAR3");
    CHECK(as_str(driver->calls[0].params.at(3)) == "EpicGame");
    CHECK(as_int(driver->calls[0].params.at(6)) ==
          static_cast<std::int64_t>(GameState::InProgress));

    CHECK(driver->calls[1].sql.find("DELETE FROM game_players WHERE game_id = ?")
          != std::string::npos);
    CHECK(driver->calls[2].sql.find("INSERT INTO game_players") != std::string::npos);
    CHECK(as_int(driver->calls[2].params.at(2)) == 0);  // position
    CHECK(as_int(driver->calls[3].params.at(2)) == 1);  // position
}

TEST_CASE("SqlGameRepository::remove deletes players then game by name",
          "[infra][persistence][game]") {
    auto driver = make_driver();
    SqlGameRepository repo{driver};

    REQUIRE(repo.remove("EpicGame").has_value());
    CHECK(driver->begin_count == 1);
    CHECK(driver->commit_count == 1);
    REQUIRE(driver->calls.size() == 2);
    CHECK(driver->calls[0].sql.find("DELETE FROM game_players WHERE game_id IN")
          != std::string::npos);
    CHECK(as_str(driver->calls[0].params.at(0)) == "EpicGame");
    CHECK(driver->calls[1].sql.find("DELETE FROM games WHERE name = ?")
          != std::string::npos);
}

TEST_CASE("SqlGameRepository::list_active excludes finalized and loads each game",
          "[infra][persistence][game]") {
    auto driver = make_driver();
    SqlGameRepository repo{driver};

    // Header query → two active games; then a player query per game.
    driver->push_result_set({
        game_row(5, 1, "WAR3", "G1", "m1", 8, static_cast<std::int64_t>(GameState::Open)),
        game_row(6, 2, "WAR3", "G2", "m2", 8, static_cast<std::int64_t>(GameState::InProgress)),
    });
    driver->push_result_set({player_row(1)});
    driver->push_result_set({player_row(2), player_row(3)});

    auto r = repo.list_active();
    REQUIRE(r.has_value());
    REQUIRE(r.value().size() == 2);
    CHECK(r.value()[0]->id().value() == 5u);
    CHECK(r.value()[0]->players().size() == 1);
    CHECK(r.value()[1]->id().value() == 6u);
    CHECK(r.value()[1]->players().size() == 2);

    // Header query filters out finalized games.
    CHECK(driver->calls[0].sql.find("FROM games WHERE state <> ?")
          != std::string::npos);
    CHECK(as_int(driver->calls[0].params.at(0)) ==
          static_cast<std::int64_t>(GameState::Finalized));
}
