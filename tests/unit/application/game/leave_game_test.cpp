// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::LeaveGame`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/leave_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::LeaveGame;
using application::game::LeaveGameError;

struct Fixture {
    infra::storage::InMemoryGameRepository games;
    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::AccountId charlie_id{3};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void setup_game_with_players(domain::GameId game_id,
                                  std::vector<domain::AccountId> players) {
        auto g = domain::gameplay::Game::host(
            game_id, players[0], star_tag,
            domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
            .value();
        for (size_t i = 1; i < players.size(); ++i) {
            (void)g.join(players[i]);
        }
        REQUIRE(games.save(g));
    }

    LeaveGame make_use_case() {
        return LeaveGame{games};
    }
};

}  // namespace

TEST_CASE("LeaveGame: leaving a game removes the player",
          "[application][game][leave]") {
    Fixture f;
    f.setup_game_with_players(domain::GameId{1}, {f.alice_id, f.bob_id});

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.bob_id);

    REQUIRE(r);
    // Verify Bob was removed
    auto g = f.games.find_by_id(1);  // Pass uint32_t, not GameId
    REQUIRE(g);
    auto players = g.value()->players();  // Use players() method, dereference shared_ptr
    REQUIRE(players.size() == 1);  // Only Alice remains
}

TEST_CASE("LeaveGame: empty game is removed from repository",
          "[application][game][leave]") {
    Fixture f;
    f.setup_game_with_players(domain::GameId{1}, {f.alice_id});
    // Verify game was saved
    auto initial = f.games.find_by_id(1);
    REQUIRE(initial);

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.alice_id);

    REQUIRE(r);
    REQUIRE(r.value().game_deleted);
    // Verify game was removed
    auto after = f.games.find_by_id(1);
    REQUIRE_FALSE(after);
}

TEST_CASE("LeaveGame: if host leaves, host migrates to another player",
          "[application][game][leave]") {
    Fixture f;
    f.setup_game_with_players(domain::GameId{1}, {f.alice_id, f.bob_id, f.charlie_id});

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.alice_id);  // Alice is the host

    REQUIRE(r);
    REQUIRE(r.value().host_migrated);
    REQUIRE(r.value().new_host.value() > 0);

    // Verify new host is one of the remaining players
    REQUIRE((r.value().new_host == f.bob_id || r.value().new_host == f.charlie_id));

    // Regression: the migration must be reflected in the *persisted* aggregate,
    // not just the returned new_host. Previously Game::leave never reassigned
    // host_, so the saved game still named the departed host (alice) — breaking
    // any later host()-keyed permission/cancel logic. Re-load and assert.
    auto reloaded = f.games.find_by_id(1);
    REQUIRE(reloaded);
    CHECK((*reloaded)->host() == r.value().new_host);
    CHECK((*reloaded)->host() != f.alice_id);
    CHECK_FALSE((*reloaded)->contains(f.alice_id));
}

TEST_CASE("LeaveGame: leaving non-existent game returns GameNotFound",
          "[application][game][leave]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{999}, f.alice_id);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveGameError::GameNotFound);
}

TEST_CASE("LeaveGame: leaving when not in game returns NotInGame",
          "[application][game][leave]") {
    Fixture f;
    f.setup_game_with_players(domain::GameId{1}, {f.alice_id, f.bob_id});

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.charlie_id);  // Charlie is not in the game

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveGameError::NotInGame);
}
