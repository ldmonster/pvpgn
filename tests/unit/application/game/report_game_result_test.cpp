// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::ReportGameResult`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/report_game_result.hpp"
#include "core/clock.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::ReportGameResult;
using application::game::ReportGameResultError;
using application::game::ReportGameResultRequest;
using application::game::PlayerResult;

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryGameRepository> games =
        std::make_shared<infra::inmemory::InMemoryGameRepository>();
    std::shared_ptr<infra::inmemory::InMemoryLadderRepository> ladder =
        std::make_shared<infra::inmemory::InMemoryLadderRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();
    core::SystemTime  now      = core::SystemTime{};

    /// Create a game in InProgress state (host has started it).
    void setup_game_in_progress(domain::GameId game_id) {
        auto g = domain::gameplay::Game::host(
            game_id, alice_id, star_tag,
            domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
            .value();
        (void)g.join(bob_id);
        (void)g.start(alice_id, now);
        REQUIRE(games->save(g));
    }

    ReportGameResult make_use_case() {
        return ReportGameResult{games, ladder, bus};
    }

    ReportGameResultRequest make_request(domain::GameId game_id) {
        return ReportGameResultRequest{
            .game_id     = game_id,
            .reporter_id = alice_id,
            .results     = {
                PlayerResult{alice_id, /*won=*/true,  /*disconnected=*/false},
                PlayerResult{bob_id,   /*won=*/false, /*disconnected=*/false},
            },
            .reported_at = now,
        };
    }
};

}  // namespace

TEST_CASE("ReportGameResult: happy path finalizes game successfully",
          "[application][game][report_result]") {
    Fixture f;
    f.setup_game_in_progress(domain::GameId{1});

    auto uc = f.make_use_case();
    auto r = uc.execute(f.make_request(domain::GameId{1}));

    REQUIRE(r);
}

TEST_CASE("ReportGameResult: game not found returns GameNotFound",
          "[application][game][report_result]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(f.make_request(domain::GameId{999}));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ReportGameResultError::GameNotFound);
}

TEST_CASE("ReportGameResult: empty results returns MissingResults",
          "[application][game][report_result]") {
    Fixture f;
    f.setup_game_in_progress(domain::GameId{1});

    auto uc = f.make_use_case();
    ReportGameResultRequest req{
        .game_id     = domain::GameId{1},
        .reporter_id = f.alice_id,
        .results     = {},   // empty — invalid
        .reported_at = f.now,
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ReportGameResultError::MissingResults);
}

TEST_CASE("ReportGameResult: reporter not in game returns InvalidReporter",
          "[application][game][report_result]") {
    Fixture f;
    f.setup_game_in_progress(domain::GameId{1});

    domain::AccountId outsider{99};
    auto uc = f.make_use_case();
    ReportGameResultRequest req{
        .game_id     = domain::GameId{1},
        .reporter_id = outsider,
        .results     = {PlayerResult{outsider, true, false}},
        .reported_at = f.now,
    };
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ReportGameResultError::InvalidReporter);
}
