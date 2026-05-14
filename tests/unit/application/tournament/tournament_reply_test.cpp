// SPDX-License-Identifier: GPL-2.0-or-later
//
// Tests for `pvpgn::application::tournament::build_tournament_reply`.
// Mirrors every branch of the legacy `_client_anongame_tournament`
// state machine.

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "application/tournament/tournament_reply.hpp"

namespace at = pvpgn::application::tournament;

namespace {

at::TournamentInputs base_inputs() {
    at::TournamentInputs in{};
    in.count             = 7;
    in.now               = 1'700'000'000;
    in.client_supported  = true;
    in.signed_up         = true;
    return in;
}

}  // namespace

TEST_CASE("tournament: type 0 when no tournament configured",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = 0;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.count == 7);
    REQUIRE(r.type == 0);
    REQUIRE(r.timestamp == 0);
    REQUIRE(r.countdown == 0);
}

TEST_CASE("tournament: type 0 when client unsupported",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now + 100;
    in.client_supported  = false;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 0);
}

TEST_CASE("tournament: type 0 when signup ended and user not signed up",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 1000;
    in.end_signup        = in.now - 100;
    in.end_preliminary   = in.now + 1000;
    in.signed_up         = false;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 0);
}

TEST_CASE("tournament: type 1 notice (countdown to prelim)",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now + 600;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 1);
    REQUIRE(r.countdown == 600);
    REQUIRE(r.unknown5 == 0x01);
    REQUIRE(r.selection == 2);
    REQUIRE(r.unknown3 == 0x00);
    REQUIRE(r.timestamp == at::convert_time(in.start_preliminary));
}

TEST_CASE("tournament: type 2 signup window",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 1000;
    in.end_signup        = in.now + 300;
    in.wins = 3; in.losses = 1; in.ties = 0;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 2);
    REQUIRE(r.countdown == 300);
    REQUIRE(r.wins == 3);
    REQUIRE(r.losses == 1);
    REQUIRE(r.unknown4 == 0x0828);
    REQUIRE(r.unknown3 == 0x08);
}

TEST_CASE("tournament: type 3 prelim period",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 2000;
    in.end_signup        = in.now - 100;
    in.end_preliminary   = in.now + 500;
    in.wins = 5;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 3);
    REQUIRE(r.countdown == 500);
    REQUIRE(r.wins == 5);
    REQUIRE(r.timestamp == at::convert_time(in.end_preliminary));
}

TEST_CASE("tournament: type 4 prelim over awaiting finals",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 3000;
    in.end_signup        = in.now - 2000;
    in.end_preliminary   = in.now - 1000;
    in.start_round_1     = in.now + 200;
    in.game_in_progress  = true;
    in.wins = 4; in.losses = 2;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 4);
    REQUIRE(r.countdown == 200);
    REQUIRE(r.wins == 4);
    REQUIRE(r.losses == 2);
}

TEST_CASE("tournament: type 5 eliminated (no finals)",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 3000;
    in.end_signup        = in.now - 2000;
    in.end_preliminary   = in.now - 1000;
    in.start_round_1     = in.now - 500;
    in.game_in_progress  = false;
    in.in_finals         = false;
    in.wins = 2; in.losses = 5;
    auto r = at::build_tournament_reply(in);
    REQUIRE(r.type == 5);
    REQUIRE(r.timestamp == 0);
    REQUIRE(r.countdown == 0);
    REQUIRE(r.unknown3 == 0x04);
    REQUIRE(r.wins == 2);
    REQUIRE(r.losses == 5);
}

TEST_CASE("tournament: convert_time matches legacy formula",
          "[application][tournament]") {
    // Spot-check the formula -- legacy emitted 0 for input
    // 0x3F21CB88 (1059179400).
    REQUIRE(at::convert_time(1059179400u) == 3276999960u);
}
