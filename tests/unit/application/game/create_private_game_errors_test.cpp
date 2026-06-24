// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new error/edge-branch tests for `application::game::CreatePrivateGame`.
// Covers branches the happy-path suite misses:
//   * max_players above the 1..16 range returns MaxPlayersOutOfRange
//   * save() failure maps to PersistenceFailed

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "application/game/create_private_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::CreatePrivateGame;
using application::game::CreatePrivateGameError;
using application::game::CreatePrivateGameRequest;

/// IGameRepository whose save() always fails. find_by_id is unused here.
class SaveFailingGameRepository final
    : public domain::gameplay::IGameRepository {
public:
    core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
    find_by_name(std::string_view) override {
        return core::fail(core::Error{core::StatusCode::NotFound, "n/a"});
    }

    core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
    find_by_id(std::uint32_t) override {
        return core::fail(core::Error{core::StatusCode::NotFound, "not found"});
    }

    core::Result<void, core::Error> save(const domain::gameplay::Game&) override {
        return core::fail(core::Error{core::StatusCode::Internal, "save failed"});
    }

    core::Result<void, core::Error> remove(std::string_view) override {
        return core::fail(core::Error{core::StatusCode::Internal, "remove failed"});
    }

    core::Result<std::vector<std::shared_ptr<domain::gameplay::Game>>, core::Error>
    list_active() override {
        return std::vector<std::shared_ptr<domain::gameplay::Game>>{};
    }
};

CreatePrivateGameRequest make_request(std::uint32_t max_players) {
    return CreatePrivateGameRequest{
        .host_id     = domain::AccountId{1},
        .game_name   = "PrivateGame",
        .password    = "secret",
        .client_tag  = domain::ClientTag::parse("STAR").value(),
        .map_name    = "Deathstar",
        .max_players = max_players,
    };
}

}  // namespace

TEST_CASE("CreatePrivateGame: max_players above 16 returns MaxPlayersOutOfRange",
          "[application][game][create_private]") {
    auto games = std::make_shared<infra::inmemory::InMemoryGameRepository>();
    auto bus   = std::make_shared<infra::inmemory::InMemoryEventBus>();

    CreatePrivateGame uc{games, bus};
    auto r = uc.execute(make_request(/*max_players=*/17));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreatePrivateGameError::MaxPlayersOutOfRange);
}

TEST_CASE("CreatePrivateGame: save failure returns PersistenceFailed",
          "[application][game][create_private]") {
    auto games = std::make_shared<SaveFailingGameRepository>();
    auto bus   = std::make_shared<infra::inmemory::InMemoryEventBus>();

    CreatePrivateGame uc{games, bus};
    auto r = uc.execute(make_request(/*max_players=*/4));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == CreatePrivateGameError::PersistenceFailed);
}
