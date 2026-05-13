// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/gameplay/game.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::ClientTag;
using domain::GameId;
using domain::MatchOutcome;
using domain::PlayerResult;
using domain::gameplay::Game;
using domain::gameplay::GameDescriptor;
using domain::gameplay::GameState;

namespace {
const ClientTag kStar = ClientTag::parse("STAR").value();

Game make_game(AccountId host = AccountId{1}, std::uint8_t max = 4) {
    GameDescriptor d{};
    d.name = "TestGame";
    d.map  = "Lost Temple";
    d.max_players = max;
    return Game::host(GameId{42}, host, kStar, d).value();
}
}  // namespace

TEST_CASE("Game::host validates input and seats the host",
          "[domain][gameplay]") {
    GameDescriptor bad{};
    REQUIRE_FALSE(Game::host(GameId{1}, AccountId{1}, kStar, bad).has_value());
    bad.name = "x"; bad.max_players = 99;
    REQUIRE_FALSE(Game::host(GameId{1}, AccountId{1}, kStar, bad).has_value());

    auto g = make_game();
    REQUIRE(g.state() == GameState::Open);
    REQUIRE(g.player_count() == 1);

    auto evs = g.drain_events();
    REQUIRE(evs.size() == 2);
    REQUIRE(std::holds_alternative<domain::events::GameCreated>(evs[0]));
    REQUIRE(std::holds_alternative<domain::events::GamePlayerJoined>(evs[1]));
}

TEST_CASE("Game: join / leave / capacity / closed-state",
          "[domain][gameplay]") {
    auto g = make_game(AccountId{1}, 2);
    (void)g.drain_events();

    REQUIRE(g.join(AccountId{2}) == Game::JoinOutcome::Joined);
    REQUIRE(g.join(AccountId{2}) == Game::JoinOutcome::AlreadyIn);
    REQUIRE(g.join(AccountId{3}) == Game::JoinOutcome::Full);

    REQUIRE(g.leave(AccountId{2}));
    REQUIRE_FALSE(g.leave(AccountId{99}));
}

TEST_CASE("Game: start FSM — host only, Open → InProgress",
          "[domain][gameplay][fsm]") {
    auto g = make_game(AccountId{1});
    (void)g.drain_events();
    const auto now = std::chrono::system_clock::time_point{};

    REQUIRE(g.start(AccountId{99}, now) == Game::StartOutcome::NotHost);
    REQUIRE(g.state() == GameState::Open);
    REQUIRE(g.start(AccountId{1}, now) == Game::StartOutcome::Started);
    REQUIRE(g.state() == GameState::InProgress);
    REQUIRE(g.start(AccountId{1}, now) == Game::StartOutcome::WrongState);

    auto evs = g.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::GameStarted>(evs[0]));
}

TEST_CASE("Game: finalize emits GameEnded with full MatchReport",
          "[domain][gameplay][fsm]") {
    auto g = make_game(AccountId{1}, 2);
    (void)g.join(AccountId{2});
    const auto now = std::chrono::system_clock::time_point{};
    (void)g.start(AccountId{1}, now);
    (void)g.drain_events();

    std::vector<PlayerResult> r = {
        {AccountId{1}, MatchOutcome::Win},
        {AccountId{2}, MatchOutcome::Loss},
    };
    REQUIRE(g.finalize(r, now) == Game::EndOutcome::Finalized);
    REQUIRE(g.state() == GameState::Finalized);

    auto evs = g.drain_events();
    REQUIRE(evs.size() == 1);
    const auto& ended = std::get<domain::events::GameEnded>(evs[0]);
    REQUIRE(ended.report.results.size() == 2);
    REQUIRE(ended.report.client == kStar);
    REQUIRE(g.finalize(r, now) == Game::EndOutcome::NotInProgress);
}
