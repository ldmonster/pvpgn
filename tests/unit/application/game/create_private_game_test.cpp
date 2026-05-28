// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::CreatePrivateGame`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/create_private_game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::CreatePrivateGame;
using application::game::CreatePrivateGameError;
using application::game::CreatePrivateGameRequest;

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryGameRepository> games =
        std::make_shared<infra::inmemory::InMemoryGameRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId alice_id{1};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    CreatePrivateGame make_use_case() {
        return CreatePrivateGame{games, bus};
    }

    CreatePrivateGameRequest make_request(std::string name = "PrivateGame",
                                          std::string password = "secret",
                                          std::uint32_t max_players = 4) {
        return CreatePrivateGameRequest{
            .host_id     = alice_id,
            .game_name   = std::move(name),
            .password    = std::move(password),
            .client_tag  = star_tag,
            .map_name    = "Deathstar",
            .max_players = max_players,
        };
    }
};

}  // namespace

TEST_CASE("CreatePrivateGame: happy path creates game and returns id",
          "[application][game][create_private]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.make_request());

    REQUIRE(r);
    REQUIRE(r.value().value() > 0);

    // Game must be retrievable from the repository
    auto found = f.games->find_by_id(r.value().value());
    REQUIRE(found);
}

TEST_CASE("CreatePrivateGame: empty game name returns InvalidGameName",
          "[application][game][create_private]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.make_request("" /* empty name */));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreatePrivateGameError::InvalidGameName);
}

TEST_CASE("CreatePrivateGame: empty password returns InvalidPassword",
          "[application][game][create_private]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.make_request("PrivateGame", "" /* empty password */));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreatePrivateGameError::InvalidPassword);
}

TEST_CASE("CreatePrivateGame: zero max_players returns MaxPlayersOutOfRange",
          "[application][game][create_private]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.make_request("PrivateGame", "secret", 0));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreatePrivateGameError::MaxPlayersOutOfRange);
}
