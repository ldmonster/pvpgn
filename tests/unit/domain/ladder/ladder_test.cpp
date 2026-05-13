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
using domain::ladder::LadderCalculator;
using domain::ladder::LadderEntry;
using domain::ladder::LadderRules;

TEST_CASE("LadderCalculator: equal-rated 1v1 → symmetric ±16 with K=32",
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
