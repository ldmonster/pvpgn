// SPDX-License-Identifier: GPL-2.0-or-later
//
// Net-new edge/branch tests for `build_tournament_reply` covering arms not
// reached by tournament_reply_test.cpp:
//   * the final type-5 fallback (in_finals == true) -- the legacy dead
//     "types 6/7" path that degrades to type-5 semantics.
//   * type-4 condition fails because game_in_progress is false, so the
//     state machine continues to the in_finals fallback.
//   * type-2 boundary where end_signup == now (>= comparison, zero countdown).
//   * type-1 boundary where start_preliminary == now (the >= edge).
//   * the type-0 "start_preliminary == 0" sub-condition in isolation while
//     the user IS signed up and the client IS supported.

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

TEST_CASE("tournament: in_finals fallback degrades to type 5",
          "[application][tournament]") {
    auto in = base_inputs();
    // All countdown windows are in the past so types 1-3 are skipped.
    in.start_preliminary = in.now - 5000;
    in.end_signup        = in.now - 4000;
    in.end_preliminary   = in.now - 3000;
    // Type-4 requires start_round_1 >= now AND game_in_progress; make the
    // round start in the past so type 4 is skipped too.
    in.start_round_1     = in.now - 2000;
    in.game_in_progress  = true;   // irrelevant once start_round_1 < now
    in.in_finals         = true;   // skips the `!in_finals` type-5 arm
    in.wins = 9; in.losses = 1; in.ties = 2;

    auto r = at::build_tournament_reply(in);

    REQUIRE(r.type == 5);
    REQUIRE(r.wins == 9);
    REQUIRE(r.losses == 1);
    REQUIRE(r.ties == 2);
    REQUIRE(r.unknown3 == 0x04);
    REQUIRE(r.selection == 2);
    REQUIRE(r.timestamp == 0);
    REQUIRE(r.countdown == 0);
}

TEST_CASE("tournament: type 4 skipped when no game in progress -> type 5",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 5000;
    in.end_signup        = in.now - 4000;
    in.end_preliminary   = in.now - 3000;
    in.start_round_1     = in.now + 1000;  // future, but...
    in.game_in_progress  = false;          // ...no game -> type 4 condition fails
    in.in_finals         = false;          // -> hits the !in_finals type-5 arm
    in.wins = 6; in.losses = 4;

    auto r = at::build_tournament_reply(in);

    REQUIRE(r.type == 5);
    REQUIRE(r.wins == 6);
    REQUIRE(r.losses == 4);
    REQUIRE(r.unknown3 == 0x04);
}

TEST_CASE("tournament: type 2 boundary when end_signup == now",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now - 1000;  // prelim already started
    in.end_signup        = in.now;         // exactly now -> >= holds, type 2
    in.wins = 1; in.losses = 0; in.ties = 0;

    auto r = at::build_tournament_reply(in);

    REQUIRE(r.type == 2);
    REQUIRE(r.countdown == 0);
    REQUIRE(r.unknown4 == 0x0828);
    REQUIRE(r.unknown3 == 0x08);
    REQUIRE(r.timestamp == at::convert_time(in.end_signup));
}

TEST_CASE("tournament: type 1 boundary when start_preliminary == now",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = in.now;  // exactly now -> >= holds, type 1
    in.end_signup        = in.now + 100;

    auto r = at::build_tournament_reply(in);

    REQUIRE(r.type == 1);
    REQUIRE(r.countdown == 0);
    REQUIRE(r.unknown3 == 0x00);
    REQUIRE(r.selection == 2);
    REQUIRE(r.timestamp == at::convert_time(in.start_preliminary));
}

TEST_CASE("tournament: type 0 via start_preliminary == 0 while signed up",
          "[application][tournament]") {
    auto in = base_inputs();
    in.start_preliminary = 0;       // not configured
    in.signed_up         = true;    // isolate the first OR sub-condition
    in.client_supported  = true;
    in.end_signup        = in.now + 1000;

    auto r = at::build_tournament_reply(in);

    REQUIRE(r.type == 0);
    REQUIRE(r.timestamp == 0);
    REQUIRE(r.countdown == 0);
}
