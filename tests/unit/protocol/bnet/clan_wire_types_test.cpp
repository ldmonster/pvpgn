// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/clan_wire_types.hpp"

namespace c = pvpgn::protocol::bnet::clan;

TEST_CASE("clan packet type codes match legacy",
          "[protocol][bnet][clan_wire_types]")
{
    REQUIRE(c::kClientArrangedTeamAcceptInvite    == 0xfdff);
    REQUIRE(c::kClientClanInfoReq                 == 0x82ff);
    REQUIRE(c::kServerClanInfoReply               == 0x82ff);
    REQUIRE(c::kClientClanCreateReq               == 0x70ff);
    REQUIRE(c::kServerClanCreateReply             == 0x70ff);
    REQUIRE(c::kClientClanCreateInviteReq         == 0x71ff);
    REQUIRE(c::kServerClanCreateInviteReply       == 0x71ff);
    REQUIRE(c::kServerClanCreateInviteReq         == 0x72ff);
    REQUIRE(c::kClientClanCreateInviteReply       == 0x72ff);
    REQUIRE(c::kClientClanDisbandReq              == 0x73ff);
    REQUIRE(c::kServerClanDisbandReply            == 0x73ff);
    REQUIRE(c::kClientClanMemberNewChiefReq       == 0x74ff);
    REQUIRE(c::kServerClanMemberNewChiefReply     == 0x74ff);
    REQUIRE(c::kServerClanClanAck                 == 0x75ff);
    REQUIRE(c::kServerClanQuitNotify              == 0x76ff);
    REQUIRE(c::kClientClanInviteReq               == 0x77ff);
    REQUIRE(c::kServerClanInviteReply             == 0x77ff);
    REQUIRE(c::kClientClanMemberRemoveReq         == 0x78ff);
    REQUIRE(c::kServerClanMemberRemoveReply       == 0x78ff);
    REQUIRE(c::kServerClanInviteReq               == 0x79ff);
    REQUIRE(c::kClientClanInviteReply             == 0x79ff);
    REQUIRE(c::kClientClanMemberRankUpdateReq     == 0x7aff);
    REQUIRE(c::kServerClanMemberRankUpdateReply   == 0x7aff);
    REQUIRE(c::kClientClanMotdChg                 == 0x7bff);
    REQUIRE(c::kServerClanMotdReply               == 0x7cff);
    REQUIRE(c::kClientClanMotdReq                 == 0x7cff);
    REQUIRE(c::kServerClanMemberListReply         == 0x7dff);
    REQUIRE(c::kClientClanMemberListReq           == 0x7dff);
    REQUIRE(c::kServerClanMemberRemovedNotify     == 0x7eff);
    REQUIRE(c::kServerClanMemberUpdate            == 0x7fff);
}

TEST_CASE("clan rank and presence enums match legacy",
          "[protocol][bnet][clan_wire_types]")
{
    REQUIRE(c::kRankNew       == 0x00);
    REQUIRE(c::kRankPeon      == 0x01);
    REQUIRE(c::kRankGrunt     == 0x02);
    REQUIRE(c::kRankShaman    == 0x03);
    REQUIRE(c::kRankChieftain == 0x04);

    REQUIRE(c::kPresenceOffline     == 0x00);
    REQUIRE(c::kPresenceOnline      == 0x01);
    REQUIRE(c::kPresenceChannel     == 0x02);
    REQUIRE(c::kPresenceGame        == 0x03);
    REQUIRE(c::kPresencePrivateGame == 0x04);
}

TEST_CASE("clan reply / response codes match legacy",
          "[protocol][bnet][clan_wire_types]")
{
    REQUIRE(c::kCreateReplyCheckOk             == 0x00);
    REQUIRE(c::kCreateReplyCheckAlreadyInUse   == 0x01);
    REQUIRE(c::kCreateReplyCheckTimeLimit      == 0x02);
    REQUIRE(c::kCreateReplyCheckException      == 0x04);
    REQUIRE(c::kCreateReplyCheckInvalidClanTag == 0x0a);

    REQUIRE(c::kDisbandReplyResultOk        == 0x00);
    REQUIRE(c::kDisbandReplyResultException == 0x01);
    REQUIRE(c::kDisbandReplyResultFailed    == 0x02);

    REQUIRE(c::kResponseSuccess       == 0x00);
    REQUIRE(c::kResponseFail          == 0x01);
    REQUIRE(c::kResponseTooSoon       == 0x02);
    REQUIRE(c::kResponseTooSmall      == 0x03);
    REQUIRE(c::kResponseDeclined      == 0x04);
    REQUIRE(c::kResponseDecline       == 0x05);
    REQUIRE(c::kResponseAccept        == 0x06);
    REQUIRE(c::kResponseNotAuthorized == 0x07);
    REQUIRE(c::kResponseNotFound      == 0x08);
    REQUIRE(c::kResponseClanFull      == 0x09);
    REQUIRE(c::kResponseBadTag        == 0x0a);
    REQUIRE(c::kResponseBadName       == 0x0b);
    REQUIRE(c::kResponseNotMember     == 0x0c);
}
