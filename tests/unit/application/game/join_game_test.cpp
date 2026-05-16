// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::JoinGame`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/join_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::JoinGame;
using application::game::JoinGameError;

struct Fixture {
    infra::storage::InMemoryGameRepository games;
    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::AccountId charlie_id{3};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void setup_game(domain::GameId game_id, std::uint8_t max_players) {
        auto g = domain::gameplay::Game::host(
            game_id, alice_id, star_tag,
            domain::gameplay::GameDescriptor{"TestGame", "Deathstar", max_players})
            .value();
        REQUIRE(games.save(g));
    }

    JoinGame make_use_case() {
        return JoinGame{games};
    }
};

}  // namespace

TEST_CASE("JoinGame: joining a game succeeds when game is open and has capacity",
          "[application][game][join]") {
    Fixture f;
    f.setup_game(domain::GameId{1}, 4);

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.bob_id);

    REQUIRE(r);
    REQUIRE(!r.value().server_address.empty());
    // Port is populated by infrastructure layer, not application layer
    REQUIRE(r.value().server_address == "127.0.0.1");
}

TEST_CASE("JoinGame: joining non-existent game returns GameNotFound",
          "[application][game][join]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{999}, f.bob_id);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinGameError::GameNotFound);
}

TEST_CASE("JoinGame: joining full game returns GameFull",
          "[application][game][join]") {
    Fixture f;
    f.setup_game(domain::GameId{1}, 2);

    auto uc = f.make_use_case();
    // Fill the game: alice is host, add bob
    (void)uc.execute(domain::GameId{1}, f.bob_id);
    // Now game is full, charlie cannot join
    auto r = uc.execute(domain::GameId{1}, f.charlie_id);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinGameError::GameFull);
}

TEST_CASE("JoinGame: game updated after successful join",
          "[application][game][join]") {
    Fixture f;
    f.setup_game(domain::GameId{1}, 4);

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.bob_id);

    REQUIRE(r);
    // Verify Bob is in the game
    auto g = f.games.find_by_id(1);  // Pass uint32_t, not GameId
    REQUIRE(g);
    auto players = g.value()->players();  // Use players() method, dereference shared_ptr
    REQUIRE(players.size() >= 2);  // Alice (host) + Bob
}

TEST_CASE("JoinGame: joining closed game returns GameClosed",
          "[application][game][join]") {
    Fixture f;
    auto g = domain::gameplay::Game::host(
        domain::GameId{1}, f.alice_id, f.star_tag,
        domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
        .value();
    // Start the game to change state from Open to InProgress (which is "closed" for joining)
    (void)g.start(f.alice_id, std::chrono::system_clock::now());
    REQUIRE(f.games.save(g));

    auto uc = f.make_use_case();
    auto r = uc.execute(domain::GameId{1}, f.bob_id);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinGameError::GameClosed);
}
