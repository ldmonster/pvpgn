// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new error/edge-branch tests for `application::game::JoinGame`.
// Covers branches the happy-path suite misses:
//   * joining a game one is already in returns AlreadyInGame
//   * save() failure after a successful join maps to GameNotFound

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "application/game/join_game.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::JoinGame;
using application::game::JoinGameError;

/// IGameRepository whose find_by_id returns a seeded game, but whose save()
/// always fails. Lets us drive the post-join persistence failure branch.
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
    infra::storage::InMemoryGameRepository games;
    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag = domain::ClientTag::parse("STAR").value();

    void setup_game(domain::GameId game_id, std::uint8_t max_players) {
        auto g = domain::gameplay::Game::host(
                     game_id, alice_id, star_tag,
                     domain::gameplay::GameDescriptor{"TestGame", "Deathstar",
                                                      max_players})
                     .value();
        REQUIRE(games.save(g));
    }
};

}  // namespace

TEST_CASE("JoinGame: joining a game you already host returns AlreadyInGame",
          "[application][game][join]") {
    Fixture f;
    f.setup_game(domain::GameId{1}, 4);

    JoinGame uc{f.games};
    // Alice is the host and therefore already a member.
    auto r = uc.execute(domain::GameId{1}, f.alice_id);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinGameError::AlreadyInGame);
}

TEST_CASE("JoinGame: save() failure after a successful join returns GameNotFound",
          "[application][game][join]") {
    domain::AccountId alice{1};
    domain::AccountId bob{2};
    domain::ClientTag star = domain::ClientTag::parse("STAR").value();

    auto g = domain::gameplay::Game::host(
                 domain::GameId{1}, alice, star,
                 domain::gameplay::GameDescriptor{"TestGame", "Deathstar", 4})
                 .value();

    SaveFailingGameRepository repo;
    repo.seed(std::make_shared<domain::gameplay::Game>(std::move(g)));

    JoinGame uc{repo};
    // bob is not yet a member, so join() succeeds and we reach the failing save().
    auto r = uc.execute(domain::GameId{1}, bob);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == JoinGameError::GameNotFound);
}
