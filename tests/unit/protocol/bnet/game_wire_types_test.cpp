// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/game_wire_types.hpp"

namespace g = pvpgn::protocol::bnet::game;

TEST_CASE("game packet type codes match legacy",
          "[protocol][bnet][game_wire_types]")
{
    REQUIRE(g::kClientGameListReq    == 0x09ff);
    REQUIRE(g::kServerGameListReply  == 0x09ff);
    REQUIRE(g::kClientStartGame1     == 0x08ff);
    REQUIRE(g::kServerStartGame1Ack  == 0x08ff);
    REQUIRE(g::kClientStartGame3     == 0x1aff);
    REQUIRE(g::kClientStartGame4     == 0x1cff);
    REQUIRE(g::kClientUnknown1b      == 0x1bff);
    REQUIRE(g::kClientCloseGame      == 0x02ff);
    REQUIRE(g::kClientCloseGame2     == 0x1fff);
    REQUIRE(g::kClientMapAuthReq1    == 0x32ff);
    REQUIRE(g::kClientMapAuthReq2    == 0x3cff);
    REQUIRE(g::kClientGameReport     == 0x2cff);
    REQUIRE(g::kClientJoinGame       == 0x22ff);
    REQUIRE(g::kClientSearchLanGames == 0x2ff7);
}

TEST_CASE("game GameListReq gametype filters match legacy",
          "[protocol][bnet][game_wire_types]")
{
    REQUIRE(g::kGameListReqAll       == 0x0000);
    REQUIRE(g::kGameListReqMelee     == 0x0002);
    REQUIRE(g::kGameListReqLadder    == 0x0009);
    REQUIRE(g::kGameListReqIronman   == 0x0010);
    REQUIRE(g::kGameListReqDiablo    == 0x0409);
    REQUIRE(g::kGameListReqLoaded    == 0x0a00);
}

TEST_CASE("game DiabloII gametype codes match legacy",
          "[protocol][bnet][game_wire_types]")
{
    REQUIRE(g::kGameTypeDiablo2Close                  == 0x00000000);
    REQUIRE(g::kGameTypeDiablo2OpenNormal             == 0x00000008);
    REQUIRE(g::kGameTypeDiablo2OpenHell               == 0x0000000a);
    REQUIRE(g::kGameTypeDiablo2OpenHardcoreHell       == 0x0000000e);
}

TEST_CASE("game StartGame4 status bits match legacy",
          "[protocol][bnet][game_wire_types]")
{
    REQUIRE(g::kStartGame4StatusInit       == 0x00000000);
    REQUIRE(g::kStartGame4StatusPrivate    == 0x00000001);
    REQUIRE(g::kStartGame4StatusFull       == 0x00000002);
    REQUIRE(g::kStartGame4StatusOpen       == 0x00000004);
    REQUIRE(g::kStartGame4StatusStart      == 0x00000008);
    REQUIRE(g::kStartGame4StatusDiscIsLoss == 0x00000010);
    REQUIRE(g::kStartGame4StatusReplay     == 0x00000080);
    REQUIRE(g::kStartGame4FlagPrivate      == 0x0001);
}

TEST_CASE("game MapType / GameSpeed / Tileset / Difficulty match legacy",
          "[protocol][bnet][game_wire_types]")
{
    REQUIRE(g::kMapTypeSelfmade == 0);
    REQUIRE(g::kMapTypeCompUsa  == 5);

    REQUIRE(g::kGameSpeedSlowest == 0);
    REQUIRE(g::kGameSpeedNormal  == 3);
    REQUIRE(g::kGameSpeedFastest == 6);

    REQUIRE(g::kTilesetBadlands == 0);
    REQUIRE(g::kTilesetTwilight == 7);

    REQUIRE(g::kDifficultyNormal            == 1);
    REQUIRE(g::kDifficultyHardcoreHell      == 6);
}

TEST_CASE("game MapAuth reply codes match legacy",
          "[protocol][bnet][game_wire_types]")
{
    REQUIRE(g::kMapAuthReply1No       == 0x00000000);
    REQUIRE(g::kMapAuthReply1Ok       == 0x00000001);
    REQUIRE(g::kMapAuthReply1LadderOk == 0x00000002);
    REQUIRE(g::kMapAuthReply2LadderOk == 0x00000002);
}
