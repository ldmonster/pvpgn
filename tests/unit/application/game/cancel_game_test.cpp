// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::CancelGame`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/cancel_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::CancelGame;
using application::game::CancelGameCommand;
using application::game::CancelGameError;

struct Fixture {
    infra::storage::InMemoryGameRepository games;
    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void setup_game(domain::GameId game_id) {
        auto g = domain::gameplay::Game::host(
            game_id, alice_id, star_tag,
            domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
            .value();
        REQUIRE(games.save(g));
    }

    CancelGame make_use_case() {
        return CancelGame{games};
    }
};

}  // namespace

TEST_CASE("CancelGame: owner cancels game successfully",
          "[application][game][cancel]") {
    Fixture f;
    f.setup_game(domain::GameId{1});

    auto uc = f.make_use_case();
    auto r = uc.execute(CancelGameCommand{domain::GameId{1}, f.alice_id});

    REQUIRE(r);
    // Verify game was removed from repository
    auto after = f.games.find_by_id(1);
    REQUIRE_FALSE(after);
}

TEST_CASE("CancelGame: game not found returns GameNotFound",
          "[application][game][cancel]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(CancelGameCommand{domain::GameId{999}, f.alice_id});

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CancelGameError::GameNotFound);
}

TEST_CASE("CancelGame: non-owner cancel returns PermissionDenied",
          "[application][game][cancel]") {
    Fixture f;
    f.setup_game(domain::GameId{1});

    auto uc = f.make_use_case();
    // Bob is not the host — alice is
    auto r = uc.execute(CancelGameCommand{domain::GameId{1}, f.bob_id});

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CancelGameError::PermissionDenied);
    // Game must still exist
    auto still_there = f.games.find_by_id(1);
    REQUIRE(still_there);
}
