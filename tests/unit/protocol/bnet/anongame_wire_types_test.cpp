// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/anongame_wire_types.hpp"

namespace a = pvpgn::protocol::bnet::anongame;

TEST_CASE("anongame packet type codes match legacy",
          "[protocol][bnet][anongame_wire_types]")
{
    REQUIRE(a::kClientFindAnongame              == 0x44ff);
    REQUIRE(a::kServerAnongameFound             == 0x44ff);
    REQUIRE(a::kClientArrangedTeamFriendScreen  == 0x60ff);
    REQUIRE(a::kClientArrangedTeamInviteFriend  == 0x61ff);
    REQUIRE(a::kServerArrangedTeamMemberDecline == 0x62ff);
    REQUIRE(a::kServerArrangedTeamSendInvite    == 0x63ff);
    REQUIRE(a::kClientFriendsListReq            == 0x65ff);
    REQUIRE(a::kClientFriendInfoReq             == 0x66ff);
    REQUIRE(a::kServerFriendAddAck              == 0x67ff);
    REQUIRE(a::kServerFriendDelAck              == 0x68ff);
    REQUIRE(a::kServerFriendMoveAck             == 0x69ff);
}

TEST_CASE("anongame client option bytes match legacy",
          "[protocol][bnet][anongame_wire_types]")
{
    REQUIRE(a::kClientFindAnongameSearch          == 0x00);
    REQUIRE(a::kClientFindAnongameInfos           == 0x02);
    REQUIRE(a::kClientFindAnongameCancel          == 0x03);
    REQUIRE(a::kClientFindAnongameProfile         == 0x04);
    REQUIRE(a::kClientFindAnongameAtSearch        == 0x05);
    REQUIRE(a::kClientFindAnongameAtInviterSearch == 0x06);
    REQUIRE(a::kClientAnongameTournament          == 0x07);
    REQUIRE(a::kClientFindAnongameProfileClan     == 0x08);
    REQUIRE(a::kClientFindAnongameGetIcon         == 0x09);
    REQUIRE(a::kClientFindAnongameSetIcon         == 0x0A);
}

TEST_CASE("anongame info-tag ASCII constants match legacy",
          "[protocol][bnet][anongame_wire_types]")
{
    REQUIRE(a::kClientFindAnongameInfoTagUrl  == 0x55524cu);
    REQUIRE(a::kClientFindAnongameInfoTagMap  == 0x4d4150u);
    REQUIRE(a::kClientFindAnongameInfoTagType == 0x54595045u);
    REQUIRE(a::kClientFindAnongameInfoTagDesc == 0x44455343u);
    REQUIRE(a::kClientFindAnongameInfoTagLadr == 0x4c414452u);
    REQUIRE(a::kServerAnongameSoloStr         == 0x534F4C4Fu);
    REQUIRE(a::kServerAnongameTeamStr         == 0x5445414Du);
    REQUIRE(a::kServerAnongameAt2v2Str        == 0x32565332u);
    REQUIRE(a::kServerAnongameAt4v4Str        == 0x34565334u);
    REQUIRE(a::kServerAnongameTyStr           == 0x54592020u);
}

TEST_CASE("anongame type enum matches legacy",
          "[protocol][bnet][anongame_wire_types]")
{
    REQUIRE(a::kAnongameType1v1      == 0);
    REQUIRE(a::kAnongameType4v4      == 3);
    REQUIRE(a::kAnongameTypeSmallFfa == 4);
    REQUIRE(a::kAnongameTypeAt2v2    == 5);
    REQUIRE(a::kAnongameTypeTeamFfa  == 6);
    REQUIRE(a::kAnongameTypeAt3v3    == 7);
    REQUIRE(a::kAnongameTypeAt4v4    == 8);
    REQUIRE(a::kAnongameTypeTy       == 9);
    REQUIRE(a::kAnongameType6v6      == 11);
    REQUIRE(a::kAnongameTypeAt2v2v2  == 17);
    REQUIRE(a::kAnongameTypes        == 18);
}

TEST_CASE("anongame friend type/status bytes match legacy",
          "[protocol][bnet][anongame_wire_types]")
{
    REQUIRE(a::kFriendTypeNonMutual == 0x00);
    REQUIRE(a::kFriendTypeMutual    == 0x01);
    REQUIRE(a::kFriendTypeDnd       == 0x02);
    REQUIRE(a::kFriendTypeAway      == 0x04);
    REQUIRE(a::kFriendStatusOffline     == 0x00);
    REQUIRE(a::kFriendStatusOnline      == 0x01);
    REQUIRE(a::kFriendStatusChat        == 0x02);
    REQUIRE(a::kFriendStatusPublicGame  == 0x03);
    REQUIRE(a::kFriendStatusPrivateGame == 0x05);
}

TEST_CASE("anongame arranged-team action codes match legacy",
          "[protocol][bnet][anongame_wire_types]")
{
    REQUIRE(a::kClientArrangedTeamAccept  == 0x00000003);
    REQUIRE(a::kClientArrangedTeamDecline == 0x00000002);
    REQUIRE(a::kServerArrangedTeamAccept  == 0x00000003);
    REQUIRE(a::kServerArrangedTeamDecline == 0x00000002);
    REQUIRE(a::kServerArrangedTeamAddName == 0x01);
}
