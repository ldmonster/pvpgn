// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/chat_wire_types.hpp"

namespace c = pvpgn::protocol::bnet::chat;

TEST_CASE("chat packet type codes match legacy",
          "[protocol][bnet][chat_wire_types]")
{
    REQUIRE(c::kClientStatsReq          == 0x26ff);
    REQUIRE(c::kServerStatsReply        == 0x26ff);
    REQUIRE(c::kClientPlayerInfoReq     == 0x0aff);
    REQUIRE(c::kServerPlayerInfoReply   == 0x0aff);
    REQUIRE(c::kClientProgIdent2        == 0x0bff);
    REQUIRE(c::kClientJoinChannel       == 0x0cff);
    REQUIRE(c::kServerChannelList       == 0x0bff);
    REQUIRE(c::kServerServerList        == 0x04ff);
    REQUIRE(c::kServerMessage           == 0x0fff);
    REQUIRE(c::kClientMessage           == 0x0eff);
    REQUIRE(c::kClientLeaveChannel      == 0x10ff);
    REQUIRE(c::kClientProfileReq        == 0x35ff);
    REQUIRE(c::kServerProfileReply      == 0x35ff);
    REQUIRE(c::kClientUnknown37         == 0x37ff);
    REQUIRE(c::kServerUnknown37         == 0x37ff);
    REQUIRE(c::kClientUnknown39         == 0x39ff);
    REQUIRE(c::kClientLadderSearchReq   == 0x2fff);
    REQUIRE(c::kServerLadderSearchReply == 0x2fff);
    REQUIRE(c::kClientLadderReq         == 0x2eff);
    REQUIRE(c::kServerLadderReply       == 0x2eff);
    REQUIRE(c::kClientStatsUpdate       == 0x27ff);
}

TEST_CASE("chat JoinChannel flags and ServerMessage magic match legacy",
          "[protocol][bnet][chat_wire_types]")
{
    REQUIRE(c::kJoinChannelNormal  == 0x00000000);
    REQUIRE(c::kJoinChannelGeneric == 0x00000001);
    REQUIRE(c::kJoinChannelCreate  == 0x00000002);

    REQUIRE(c::kServerMessageRegAuth    == 0xBAADF00D);
    REQUIRE(c::kServerMessageAccountNum == 0x0df0adba);
}

TEST_CASE("chat ServerMessage type enum matches legacy",
          "[protocol][bnet][chat_wire_types]")
{
    REQUIRE(c::kServerMessageTypeAddUser             == 0x00000001);
    REQUIRE(c::kServerMessageTypeJoin                == 0x00000002);
    REQUIRE(c::kServerMessageTypePart                == 0x00000003);
    REQUIRE(c::kServerMessageTypeWhisper             == 0x00000004);
    REQUIRE(c::kServerMessageTypeTalk                == 0x00000005);
    REQUIRE(c::kServerMessageTypeBroadcast           == 0x00000006);
    REQUIRE(c::kServerMessageTypeChannel             == 0x00000007);
    REQUIRE(c::kServerMessageTypeUserFlags           == 0x00000009);
    REQUIRE(c::kServerMessageTypeWhisperAck          == 0x0000000a);
    REQUIRE(c::kServerMessageTypeChannelFull         == 0x0000000d);
    REQUIRE(c::kServerMessageTypeChannelDoesNotExist == 0x0000000e);
    REQUIRE(c::kServerMessageTypeChannelRestricted   == 0x0000000f);
    REQUIRE(c::kServerMessageTypeInfo                == 0x00000012);
    REQUIRE(c::kServerMessageTypeError               == 0x00000013);
    REQUIRE(c::kServerMessageTypeEmote               == 0x00000017);
}

TEST_CASE("chat player and channel flags match legacy",
          "[protocol][bnet][chat_wire_types]")
{
    REQUIRE(c::kMfBlizzard == 0x00000001);
    REQUIRE(c::kMfGavel    == 0x00000002);
    REQUIRE(c::kMfVoice    == 0x00000004);
    REQUIRE(c::kMfBnet     == 0x00000008);
    REQUIRE(c::kMfPlug     == 0x00000010);
    REQUIRE(c::kMfX        == 0x00000020);
    REQUIRE(c::kMfShades   == 0x00000040);
    REQUIRE(c::kMfBeep     == 0x00000100);
    REQUIRE(c::kMfPglPlay  == 0x00000200);
    REQUIRE(c::kMfPglOffl  == 0x00000400);
    REQUIRE(c::kMfKbkPlay  == 0x00000800);
    REQUIRE(c::kMfKbkRef   == 0x00001000);

    REQUIRE(c::kCfPublic     == 0x00000001);
    REQUIRE(c::kCfModerated  == 0x00000002);
    REQUIRE(c::kCfRestricted == 0x00000004);
    REQUIRE(c::kCfTheVoid    == 0x00000008);
    REQUIRE(c::kCfSystem     == 0x00000020);
    REQUIRE(c::kCfOfficial   == 0x00001000);
}

TEST_CASE("chat D2 class and W3 race/icon enums match legacy",
          "[protocol][bnet][chat_wire_types]")
{
    REQUIRE(c::kD2CharInfoClassAmazon      == 0x01);
    REQUIRE(c::kD2CharInfoClassSorceress   == 0x02);
    REQUIRE(c::kD2CharInfoClassNecromancer == 0x03);
    REQUIRE(c::kD2CharInfoClassPaladin     == 0x04);
    REQUIRE(c::kD2CharInfoClassBarbarian   == 0x05);
    REQUIRE(c::kD2CharInfoClassDruid       == 0x06);
    REQUIRE(c::kD2CharInfoClassAssassin    == 0x07);

    REQUIRE(c::kW3RaceRandom     == 32);
    REQUIRE(c::kW3RaceHumans     == 1);
    REQUIRE(c::kW3RaceOrcs       == 2);
    REQUIRE(c::kW3RaceUndead     == 8);
    REQUIRE(c::kW3RaceNightelves == 4);
    REQUIRE(c::kW3RaceDemons     == 16);

    REQUIRE(c::kW3IconRandom     == 0);
    REQUIRE(c::kW3IconHumans     == 1);
    REQUIRE(c::kW3IconOrcs       == 2);
    REQUIRE(c::kW3IconUndead     == 3);
    REQUIRE(c::kW3IconNightelves == 4);
    REQUIRE(c::kW3IconDemons     == 5);
}

TEST_CASE("chat LadderSearch id/type and DRTL class enums match legacy",
          "[protocol][bnet][chat_wire_types]")
{
    REQUIRE(c::kLadderSearchReqIdStandard       == 0x00000001);
    REQUIRE(c::kLadderSearchReqIdIronman        == 0x00000003);
    REQUIRE(c::kLadderSearchReqTypeHighestRated == 0x00000000);
    REQUIRE(c::kLadderSearchReqTypeMostWins     == 0x00000002);
    REQUIRE(c::kLadderSearchReqTypeMostGames    == 0x00000003);
    REQUIRE(c::kLadderSearchReplyRankNone       == 0xffffffff);

    REQUIRE(c::kPlayerInfoDrtlClassWarrior  == 0);
    REQUIRE(c::kPlayerInfoDrtlClassRogue    == 1);
    REQUIRE(c::kPlayerInfoDrtlClassSorcerer == 2);
}
