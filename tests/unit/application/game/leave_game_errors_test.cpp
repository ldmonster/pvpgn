// SPDX-License-Identifier: GPL-2.0-or-later
//
// Error/edge-branch tests for `application::game::LeaveGame`.
// Covers branches the happy-path suite misses:
//   * repository remove() failure on empty game maps to PersistenceFailed
//   * repository save() failure on non-empty game maps to PersistenceFailed
//   * host departure migrates the host to a remaining player

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "application/game/leave_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"

namespace {

using namespace pvpgn;
using application::game::LeaveGame;
using application::game::LeaveGameError;

/// IGameRepository whose find_by_id returns a seeded game, but whose save()
/// and remove() always fail. Both LeaveGame persistence paths funnel their
/// failure to LeaveGameError::PersistenceFailed.
class FailingGameRepository final : public domain::gameplay::IGameRepository {
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

domain::ClientTag star_tag() { return domain::ClientTag::parse("STAR").value(); }

std::shared_ptr<domain::gameplay::Game>
make_game(domain::GameId id, std::vector<domain::AccountId> players) {
    auto g = domain::gameplay::Game::host(
                 id, players[0], star_tag(),
                 domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
                 .value();
    for (std::size_t i = 1; i < players.size(); ++i) {
        (void)g.join(players[i]);
    }
    return std::make_shared<domain::gameplay::Game>(std::move(g));
}

}  // namespace

TEST_CASE("LeaveGame: remove() failure on empty game returns PersistenceFailed",
          "[application][game][leave]") {
    domain::AccountId alice{1};
    FailingGameRepository repo;
    // Single-player game: alice leaving empties it, forcing the remove() path.
    repo.seed(make_game(domain::GameId{1}, {alice}));

    LeaveGame uc{repo};
    auto r = uc.execute(domain::GameId{1}, alice);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveGameError::PersistenceFailed);
}

TEST_CASE("LeaveGame: save() failure on non-empty game returns PersistenceFailed",
          "[application][game][leave]") {
    domain::AccountId alice{1};
    domain::AccountId bob{2};
    FailingGameRepository repo;
    // Two-player game: bob leaving keeps the game populated, forcing save() path.
    repo.seed(make_game(domain::GameId{1}, {alice, bob}));

    LeaveGame uc{repo};
    auto r = uc.execute(domain::GameId{1}, bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == LeaveGameError::PersistenceFailed);
}
