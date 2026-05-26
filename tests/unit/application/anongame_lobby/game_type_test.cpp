// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "application/anongame_lobby/game_type.hpp"

namespace al = pvpgn::application::anongame_lobby;

TEST_CASE("bracket_size_for_game_type: 1v1 returns 2",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(0u) == 2u);  // ANONGAME_TYPE_1V1
}

TEST_CASE("bracket_size_for_game_type: 2v2 / AT2v2 / SmallFFA return 4",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(1u) == 4u);  // 2v2
    CHECK(al::bracket_size_for_game_type(5u) == 4u);  // AT_2v2
    CHECK(al::bracket_size_for_game_type(4u) == 4u);  // SMALL_FFA
}

TEST_CASE("bracket_size_for_game_type: 3v3 family returns 6",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(2u)  == 6u);  // 3v3
    CHECK(al::bracket_size_for_game_type(7u)  == 6u);  // AT_3v3
    CHECK(al::bracket_size_for_game_type(12u) == 6u);  // 2v2v2
    CHECK(al::bracket_size_for_game_type(17u) == 6u);  // AT_2v2v2
}

TEST_CASE("bracket_size_for_game_type: 4v4 family + TeamFFA + 2v2v2v2 return 8",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(3u)  == 8u);  // 4v4
    CHECK(al::bracket_size_for_game_type(8u)  == 8u);  // AT_4v4
    CHECK(al::bracket_size_for_game_type(6u)  == 8u);  // TEAM_FFA
    CHECK(al::bracket_size_for_game_type(15u) == 8u);  // 2v2v2v2
}

TEST_CASE("bracket_size_for_game_type: 3v3v3 returns 9, 5v5 returns 10",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(13u) == 9u);
    CHECK(al::bracket_size_for_game_type(10u) == 10u);
}

TEST_CASE("bracket_size_for_game_type: 6v6 / 4v4v4 / 3v3v3v3 return 12",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(11u) == 12u);
    CHECK(al::bracket_size_for_game_type(14u) == 12u);
    CHECK(al::bracket_size_for_game_type(16u) == 12u);
}

TEST_CASE("bracket_size_for_game_type: Tournament returns 0 (dynamic)",
          "[anongame_lobby][game_type]") {
    // Legacy `_anongame_totalplayers` defers to
    // `tournament_get_totalplayers()` -- the application table
    // returns 0 here so the legacy adapter knows to override.
    CHECK(al::bracket_size_for_game_type(9u) == 0u);
}

TEST_CASE("bracket_size_for_game_type: unknown game types return 0",
          "[anongame_lobby][game_type]") {
    CHECK(al::bracket_size_for_game_type(18u)         == 0u);  // past last
    CHECK(al::bracket_size_for_game_type(100u)        == 0u);
    CHECK(al::bracket_size_for_game_type(0xFFFFFFFFu) == 0u);
}
