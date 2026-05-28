// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `application::game::ListPublicGames`. Exercises the use-case
// against the in-memory port adapters.

#include <catch2/catch_test_macros.hpp>

#include "application/game/list_public_games.hpp"
#include "domain/gameplay/game.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "infra/inmemory/game_repository.hpp"

namespace {

using namespace pvpgn;
using application::game::ListPublicGames;
using application::game::ListPublicGamesRequest;
using application::game::ListPublicGamesError;

struct Fixture {
    std::shared_ptr<infra::inmemory::InMemoryGameRepository> games =
        std::make_shared<infra::inmemory::InMemoryGameRepository>();

    domain::AccountId alice_id{1};
    domain::AccountId bob_id{2};
    domain::ClientTag star_tag  = domain::ClientTag::parse("STAR").value();
    domain::ClientTag w3xp_tag  = domain::ClientTag::parse("W3XP").value();

    void add_game(domain::GameId id, std::string name,
                  domain::ClientTag tag = domain::ClientTag::parse("STAR").value()) {
        auto g = domain::gameplay::Game::host(
            id, alice_id, tag,
            domain::gameplay::GameDescriptor{std::move(name), "Deathstar", 4})
            .value();
        REQUIRE(games->save(g));
    }

    ListPublicGames make_use_case() {
        return ListPublicGames{games};
    }
};

}  // namespace

TEST_CASE("ListPublicGames: empty repository returns empty list",
          "[application][game][list_public]") {
    Fixture f;

    auto uc = f.make_use_case();
    auto r = uc.execute(ListPublicGamesRequest{});

    REQUIRE(r);
    REQUIRE(r.value().empty());
}

TEST_CASE("ListPublicGames: multiple games are all returned",
          "[application][game][list_public]") {
    Fixture f;
    f.add_game(domain::GameId{1}, "Alpha");
    f.add_game(domain::GameId{2}, "Beta");
    f.add_game(domain::GameId{3}, "Gamma");

    auto uc = f.make_use_case();
    auto r = uc.execute(ListPublicGamesRequest{});

    REQUIRE(r);
    REQUIRE(r.value().size() == 3);
}

TEST_CASE("ListPublicGames: filter by client tag returns only matching games",
          "[application][game][list_public]") {
    Fixture f;
    f.add_game(domain::GameId{1}, "StarGame",  f.star_tag);
    f.add_game(domain::GameId{2}, "W3Game",    f.w3xp_tag);
    f.add_game(domain::GameId{3}, "StarGame2", f.star_tag);

    auto uc = f.make_use_case();
    ListPublicGamesRequest req;
    req.filter_by_tag = f.star_tag;
    auto r = uc.execute(req);

    REQUIRE(r);
    REQUIRE(r.value().size() == 2);
    for (const auto& info : r.value()) {
        REQUIRE(info.name != "W3Game");
    }
}

TEST_CASE("ListPublicGames: max_results of zero returns InvalidRequest",
          "[application][game][list_public]") {
    Fixture f;
    f.add_game(domain::GameId{1}, "Alpha");

    auto uc = f.make_use_case();
    ListPublicGamesRequest req;
    req.max_results = 0;
    auto r = uc.execute(req);

    REQUIRE_FALSE(r);
    REQUIRE(r.error() == ListPublicGamesError::InvalidRequest);
}
