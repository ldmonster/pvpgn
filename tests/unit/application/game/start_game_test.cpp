// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::StartGame`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/start_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/storage/repository/game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::StartGame;
using application::game::StartGameError;

struct Fixture {
    infra::storage::InMemoryGameRepository games;
    domain::AccountId alice_id{1};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    StartGame make_use_case() {
        return StartGame{games};
    }
};

}  // namespace

TEST_CASE("StartGame: creating a game succeeds with valid parameters",
          "[application][game][start]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, f.star_tag, "TestGame", "Deathstar", 4);

    REQUIRE(r);
    REQUIRE(!r.value().game_id.value() == 0);  // Has valid ID
}

TEST_CASE("StartGame: game is saved to repository",
          "[application][game][start]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, f.star_tag, "TestGame", "Deathstar", 4);

    REQUIRE(r);
    REQUIRE(f.games.size() == 1);
    
    // Verify the game can be retrieved
    auto g = f.games.find_by_id(r.value().game_id);
    REQUIRE(g);
}

TEST_CASE("StartGame: result contains new GameId",
          "[application][game][start]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, f.star_tag, "TestGame", "Deathstar", 4);

    REQUIRE(r);
    REQUIRE(r.value().game_id.value() > 0);
    REQUIRE(!r.value().game.id().value() == 0);
}

TEST_CASE("StartGame: invalid game name returns error",
          "[application][game][start]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, f.star_tag, "", "Deathstar", 4);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == StartGameError::InvalidGameName);
}

TEST_CASE("StartGame: max players out of range returns error",
          "[application][game][start]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, f.star_tag, "TestGame", "Deathstar", 0);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == StartGameError::MaxPlayersOutOfRange);
}

TEST_CASE("StartGame: max players too large returns error",
          "[application][game][start]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.alice_id, f.star_tag, "TestGame", "Deathstar", 100);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == StartGameError::MaxPlayersOutOfRange);
}
