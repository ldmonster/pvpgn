// SPDX-License-Identifier: GPL-2.0-or-later
#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "domain/ladder/ladder.hpp"

using namespace pvpgn;
using domain::AccountId;
using domain::ClientTag;
using domain::GameId;
using domain::MatchOutcome;
using domain::MatchReport;
using domain::PlayerResult;
using domain::ladder::apply_rating_delta;
using domain::ladder::kMinRating;
using domain::ladder::LadderCalculator;
using domain::ladder::LadderEntry;
using domain::ladder::LadderRules;

TEST_CASE("LadderEntry: fresh entry seeds rating at 1000 (BNETD_LADDER_INIT_RAT)",
          "[domain][ladder]") {
    // A default-constructed ladder entry must start at the original PvPGN
    // seed of 1000, not the chess-standard 1500.
    LadderEntry fresh{};
    REQUIRE(fresh.rating == 1000);
}

TEST_CASE("LadderCalculator: equal-rated 1v1 -> symmetric +/-16 with K=32",
          "[domain][ladder]") {
    MatchReport rep{GameId{1}, ClientTag::parse("WAR3").value(),
                    {{AccountId{1}, MatchOutcome::Win},
                     {AccountId{2}, MatchOutcome::Loss}},
                    std::chrono::system_clock::time_point{}};
    std::vector<LadderEntry> cur = {
        {AccountId{1}, 1500, 0, 0, 0},
        {AccountId{2}, 1500, 0, 0, 0},
    };
    LadderCalculator calc{};
    auto d = calc.compute(rep, cur);
    REQUIRE(d.size() == 2);
    REQUIRE(d[0].account == AccountId{1});
    REQUIRE(d[0].wins_delta == 1);
    REQUIRE(d[0].rating_delta == 16);
    REQUIRE(d[1].losses_delta == 1);
    REQUIRE(d[1].rating_delta == -16);
}

TEST_CASE("LadderCalculator: underdog upset gains more rating",
          "[domain][ladder]") {
    MatchReport rep{GameId{1}, ClientTag::parse("WAR3").value(),
                    {{AccountId{1}, MatchOutcome::Win}},
                    std::chrono::system_clock::time_point{}};
    LadderCalculator calc{};
    auto d_under = calc.compute(rep, std::vector<LadderEntry>{{AccountId{1}, 1300}});
    auto d_fav   = calc.compute(rep, std::vector<LadderEntry>{{AccountId{1}, 1700}});
    REQUIRE(d_under[0].rating_delta > d_fav[0].rating_delta);
}

TEST_CASE("LadderCalculator: disconnect counted as loss by default",
          "[domain][ladder]") {
    MatchReport rep{GameId{1}, ClientTag::parse("WAR3").value(),
                    {{AccountId{1}, MatchOutcome::Disconnect}},
                    std::chrono::system_clock::time_point{}};
    LadderCalculator calc{};
    auto d = calc.compute(rep, std::vector<LadderEntry>{{AccountId{1}, 1500}});
    REQUIRE(d[0].disconnects_delta == 1);
    REQUIRE(d[0].losses_delta == 1);
    REQUIRE(d[0].rating_delta < 0);
}

TEST_CASE("LadderCalculator: configurable K factor",
          "[domain][ladder]") {
    MatchReport rep{GameId{1}, ClientTag::parse("STAR").value(),
                    {{AccountId{1}, MatchOutcome::Win},
                     {AccountId{2}, MatchOutcome::Loss}},
                    std::chrono::system_clock::time_point{}};
    LadderCalculator calc{LadderRules{16.0, true}};
    std::vector<LadderEntry> cur = {{AccountId{1}, 1500}, {AccountId{2}, 1500}};
    auto d = calc.compute(rep, cur);
    REQUIRE(d[0].rating_delta == 8);
    REQUIRE(d[1].rating_delta == -8);
}

// --- Rating floor (finding L1): rating must never drop below kMinRating ----

TEST_CASE("apply_rating_delta: clamps the result at kMinRating",
          "[domain][ladder]") {
    REQUIRE(kMinRating == 1);
    // A delta that would take the rating to zero or negative floors at 1.
    REQUIRE(apply_rating_delta(5, -100) == kMinRating);
    REQUIRE(apply_rating_delta(1, -1) == kMinRating);
    REQUIRE(apply_rating_delta(1, 0) == kMinRating);
    // Normal deltas pass through unchanged.
    REQUIRE(apply_rating_delta(1000, -16) == 984);
    REQUIRE(apply_rating_delta(1000, 16) == 1016);
}

TEST_CASE("LadderCalculator: huge expected-loss against a far stronger "
          "opponent floors at kMinRating instead of going negative",
          "[domain][ladder]") {
    // Player at the floor (rating 1) loses to a 2000-rated opponent: the raw
    // Elo step is a large negative number; the delta must be clamped so the
    // resulting rating (cur + delta) never drops below kMinRating.
    MatchReport rep{GameId{1}, ClientTag::parse("WAR3").value(),
                    {{AccountId{1}, MatchOutcome::Loss},
                     {AccountId{2}, MatchOutcome::Win}},
                    std::chrono::system_clock::time_point{}};
    std::vector<LadderEntry> cur = {
        {AccountId{1}, kMinRating, 0, 0, 0},
        {AccountId{2}, 2000, 0, 0, 0},
    };
    LadderCalculator calc{};
    auto d = calc.compute(rep, cur);
    REQUIRE(d.size() == 2);
    REQUIRE(d[0].account == AccountId{1});
    REQUIRE(d[0].losses_delta == 1);
    // cur (1) + delta must not fall below the floor.
    REQUIRE(cur[0].rating + d[0].rating_delta >= kMinRating);
    REQUIRE(cur[0].rating + d[0].rating_delta == kMinRating);
}

TEST_CASE("LadderCalculator: a low-rated player losing many games never "
          "drops below kMinRating",
          "[domain][ladder]") {
    // Simulate a long losing streak against much stronger opponents and apply
    // each delta with the floor clamp. The rating must converge to and stay at
    // kMinRating, never going to 0 or negative.
    // Opponents are at/below the player's rating, so each loss carries a real
    // negative delta (losing to a much-higher-rated opponent is "expected" and
    // barely moves ELO — that would never approach the floor). Repeated real
    // losses must drive the rating down to kMinRating and clamp there.
    LadderCalculator calc{};
    std::int32_t rating = 50;  // already low
    for (int game = 0; game < 100; ++game) {
        MatchReport rep{GameId{static_cast<std::uint32_t>(game)},
                        ClientTag::parse("WAR3").value(),
                        {{AccountId{1}, MatchOutcome::Loss}},
                        std::chrono::system_clock::time_point{}};
        std::vector<LadderEntry> cur = {{AccountId{1}, rating, 0, 0, 0},
                                        {AccountId{2}, rating, 0, 0, 0}};
        auto d = calc.compute(rep, cur);
        rating = apply_rating_delta(rating, d[0].rating_delta);
        REQUIRE(rating >= kMinRating);
    }
    REQUIRE(rating == kMinRating);
}

TEST_CASE("LadderCalculator: a normal win/loss still adjusts within range",
          "[domain][ladder]") {
    // The floor must not interfere with ordinary mid-range rating changes.
    MatchReport rep{GameId{1}, ClientTag::parse("WAR3").value(),
                    {{AccountId{1}, MatchOutcome::Win},
                     {AccountId{2}, MatchOutcome::Loss}},
                    std::chrono::system_clock::time_point{}};
    std::vector<LadderEntry> cur = {{AccountId{1}, 1500, 0, 0, 0},
                                    {AccountId{2}, 1500, 0, 0, 0}};
    LadderCalculator calc{};
    auto d = calc.compute(rep, cur);
    // Equal-rated K=32 game: winner +16, loser -16, untouched by the floor.
    REQUIRE(d[0].rating_delta == 16);
    REQUIRE(d[1].rating_delta == -16);
    REQUIRE(apply_rating_delta(cur[0].rating, d[0].rating_delta) == 1516);
    REQUIRE(apply_rating_delta(cur[1].rating, d[1].rating_delta) == 1484);
}
