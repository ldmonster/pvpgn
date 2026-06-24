// SPDX-License-Identifier: GPL-2.0-or-later
//
// Error/edge-branch tests for `application::game::ReportGameResult`.
// Covers the uncovered branches the happy-path suite misses:
//   * begin_report() rejects a game that is not InProgress (still Open)
//   * save() failure maps to PersistenceFailed

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "application/game/report_game_result.hpp"
#include "core/clock.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/game_repository.hpp"
#include "infra/inmemory/ladder_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::PlayerResult;
using application::game::ReportGameResult;
using application::game::ReportGameResultError;
using application::game::ReportGameResultRequest;

/// IGameRepository whose find_by_id returns a stored game, but whose save()
/// always fails. Lets us exercise the PersistenceFailed branch.
class SaveFailingGameRepository final
    : public domain::gameplay::IGameRepository {
public:
    core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
    find_by_name(std::string_view) override {
        return core::fail(core::Error{core::StatusCode::NotFound, "n/a"});
    }

    core::Result<std::shared_ptr<domain::gameplay::Game>, core::Error>
    find_by_id(std::uint32_t id) override {
        if (game_ && game_->id().value() == id) {
            return game_;
        }
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

    void seed(std::shared_ptr<domain::gameplay::Game> g) { game_ = std::move(g); }

private:
    std::shared_ptr<domain::gameplay::Game> game_;
};

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryLadderRepository> ladder =
        std::make_shared<infra::inmemory::InMemoryLadderRepository>();
    std::shared_ptr<infra::inmemory::InMemoryEventBus> bus =
        std::make_shared<infra::inmemory::InMemoryEventBus>();

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();
    core::SystemTime  now      = core::SystemTime{};

    /// Game left in the Open state (host has NOT started it).
    domain::gameplay::Game make_open_game(domain::GameId game_id) {
        auto g = domain::gameplay::Game::host(
                     game_id, alice_id, star_tag,
                     domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
                     .value();
        (void)g.join(bob_id);
        return g;
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

TEST_CASE("ReportGameResult: game still Open (never started) returns GameNotInProgress",
          "[application][game][report_result]") {
    Fixture f;
    auto games = std::make_shared<infra::inmemory::InMemoryGameRepository>();
    // Save a game that is still Open — begin_report() must reject it.
    auto g = f.make_open_game(domain::GameId{1});
    REQUIRE(games->save(g));

    ReportGameResult uc{games, f.ladder, f.bus};
    auto r = uc.execute(f.make_request(domain::GameId{1}));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ReportGameResultError::GameNotInProgress);
}

TEST_CASE("ReportGameResult: save failure returns PersistenceFailed",
          "[application][game][report_result]") {
    Fixture f;
    auto games = std::make_shared<SaveFailingGameRepository>();

    // Seed an InProgress game so begin_report()/finalize() succeed but save() fails.
    auto g = std::make_shared<domain::gameplay::Game>(
        f.make_open_game(domain::GameId{1}));
    (void)g->start(f.alice_id, f.now);
    games->seed(g);

    ReportGameResult uc{games, f.ladder, f.bus};
    auto r = uc.execute(f.make_request(domain::GameId{1}));

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ReportGameResultError::PersistenceFailed);
}
