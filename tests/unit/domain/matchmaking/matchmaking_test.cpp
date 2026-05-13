// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include "domain/matchmaking/anon_game_queue.hpp"
#include "domain/matchmaking/tournament.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::ClientTag;
using domain::GameId;
using domain::matchmaking::AnonGameQueue;
using domain::matchmaking::Tournament;

namespace {
const auto kT0 = std::chrono::system_clock::time_point{};
const ClientTag kStar = ClientTag::parse("WAR3").value();
}

TEST_CASE("AnonGameQueue: enqueue is idempotent per account",
          "[domain][matchmaking][queue]") {
    AnonGameQueue q{kStar, 1};
    REQUIRE(q.enqueue(AccountId{1}, kT0));
    REQUIRE_FALSE(q.enqueue(AccountId{1}, kT0));
    REQUIRE(q.size() == 1);
}

TEST_CASE("AnonGameQueue: match pulls 2*team_size oldest entries",
          "[domain][matchmaking][queue]") {
    AnonGameQueue q{kStar, 2};
    for (std::uint32_t i = 1; i <= 3; ++i) {
        REQUIRE(q.enqueue(AccountId{i}, kT0));
    }
    REQUIRE_FALSE(q.can_match());
    REQUIRE(q.enqueue(AccountId{4}, kT0));
    REQUIRE(q.enqueue(AccountId{5}, kT0));
    REQUIRE(q.can_match());
    (void)q.drain_events();

    auto picked = q.match(GameId{1});
    REQUIRE(picked.size() == 4);
    REQUIRE(picked.front() == AccountId{1});
    REQUIRE(q.size() == 1);

    auto evs = q.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::AnonGameMatched>(evs[0]));
}

TEST_CASE("AnonGameQueue: dequeue emits AnonGameDequeued",
          "[domain][matchmaking][queue]") {
    AnonGameQueue q{kStar, 1};
    (void)q.enqueue(AccountId{1}, kT0);
    (void)q.drain_events();
    REQUIRE_FALSE(q.dequeue(AccountId{99}));
    REQUIRE(q.dequeue(AccountId{1}));
    auto evs = q.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::AnonGameDequeued>(evs[0]));
}

TEST_CASE("Tournament::schedule rejects <2 participants and emits event",
          "[domain][matchmaking][tournament]") {
    REQUIRE_FALSE(Tournament::schedule(1, kStar, {AccountId{1}}, kT0).has_value());
    auto t = Tournament::schedule(1, kStar,
        {AccountId{1}, AccountId{2}, AccountId{3}, AccountId{4}}, kT0).value();
    REQUIRE(t.participant_count() == 4);
    auto evs = t.drain_events();
    REQUIRE(evs.size() == 1);
    REQUIRE(std::holds_alternative<domain::events::TournamentScheduled>(evs[0]));
}
