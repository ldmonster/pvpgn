// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/d2gs/game_wire_types.hpp"

namespace g = pvpgn::protocol::d2gs::game;

TEST_CASE("d2gs::game message type codes match legacy",
          "[protocol][d2gs][game_wire_types]")
{
    REQUIRE(g::kServer00            == 0x00);
    REQUIRE(g::kClient01            == 0x01);
    REQUIRE(g::kServerJoinOk        == 0x01);
    REQUIRE(g::kClientChatMessage   == 0x15);
    REQUIRE(g::kServerNoop          == 0x20);
    REQUIRE(g::kServerChatMessage   == 0x26);
    REQUIRE(g::kClientDie           == 0x41);
    REQUIRE(g::kServerUnknown59     == 0x59);
    REQUIRE(g::kServerJoinGameMsg   == 0x5a);
    REQUIRE(g::kClientCreateGameReq == 0x60);
    REQUIRE(g::kClientJoinGameReq   == 0x61);
    REQUIRE(g::kClientQuitGame      == 0x62);
    REQUIRE(g::kClientJoinActReq    == 0x64);
    REQUIRE(g::kClientPlayerSave    == 0x65);
    REQUIRE(g::kClientUnknown66     == 0x66);
    REQUIRE(g::kServerUnknown8F     == 0x8f);
    REQUIRE(g::kServerUnknown96     == 0x96);
    REQUIRE(g::kServerWelcome       == 0x97);
    REQUIRE(g::kServerCloseGame     == 0x98);
    REQUIRE(g::kServerPlayerSave    == 0x9b);
    REQUIRE(g::kServerError         == 0x9c);
}

TEST_CASE("d2gs::game error codes match legacy",
          "[protocol][d2gs][game_wire_types]")
{
    REQUIRE(g::kErrorUnknownFailure == 0);
    REQUIRE(g::kErrorCharVer        == 1);
    REQUIRE(g::kErrorGameFull       == 15);
    REQUIRE(g::kErrorGameVer        == 16);
    REQUIRE(g::kErrorNightmare      == 17);
    REQUIRE(g::kErrorHell           == 18);
    REQUIRE(g::kErrorNormalHardcore == 19);
    REQUIRE(g::kErrorHardcoreNormal == 20);
    REQUIRE(g::kErrorDeadHardcore   == 21);
}

TEST_CASE("d2gs::game chat magic constants",
          "[protocol][d2gs][game_wire_types]")
{
    REQUIRE(g::kServerChatMessageUnknown1 == 0x0001);
    REQUIRE(g::kServerChatMessageUnknown2 == 0x00000002);
    REQUIRE(g::kServerChatMessageUnknown3 == 0x0000);
    REQUIRE(g::kServerChatMessageUnknown4 == 0x01);
}

TEST_CASE("d2gs::game structs default-init to zero",
          "[protocol][d2gs][game_wire_types]")
{
    g::ClientCreateGameReq req{};
    REQUIRE(req.servertype == 0);
    REQUIRE(req.gameflag   == 0);
    REQUIRE(req.gamename[0] == '\0');
    REQUIRE(req.charname[15] == '\0');

    g::ClientJoinGameReq jr{};
    REQUIRE(jr.charclass == 0);
    REQUIRE(jr.charname[0] == '\0');

    g::ServerUnknown8F u{};
    REQUIRE(u.unknown7 == 0);
}
