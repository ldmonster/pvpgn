// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/d2cs/wire_types.hpp"

namespace w = pvpgn::protocol::d2cs::wire;

TEST_CASE("d2cs::wire message type codes match legacy",
          "[protocol][d2cs][wire_types]")
{
    REQUIRE(w::kClientLoginReq          == 0x01);
    REQUIRE(w::kClientCreateCharReq     == 0x02);
    REQUIRE(w::kClientCreateGameReq     == 0x03);
    REQUIRE(w::kClientJoinGameReq       == 0x04);
    REQUIRE(w::kClientGameListReq       == 0x05);
    REQUIRE(w::kClientGameInfoReq       == 0x06);
    REQUIRE(w::kClientCharLoginReq      == 0x07);
    REQUIRE(w::kClientDeleteCharReq     == 0x0a);
    REQUIRE(w::kClientLadderReq         == 0x11);
    REQUIRE(w::kClientMotdReq           == 0x12);
    REQUIRE(w::kClientCancelCreateGame  == 0x13);
    REQUIRE(w::kCreateGameWait          == 0x14);
    REQUIRE(w::kCharLadderReq           == 0x16);
    REQUIRE(w::kClientCharListReq       == 0x17);
    REQUIRE(w::kClientConvertCharReq    == 0x18);
    REQUIRE(w::kClientCharListReq110    == 0x19);
}

TEST_CASE("d2cs::wire reply codes match legacy",
          "[protocol][d2cs][wire_types]")
{
    REQUIRE(w::kLoginReplyBadPass             == 0x0c);
    REQUIRE(w::kCreateCharReplyAlreadyExist   == 0x14);
    REQUIRE(w::kCreateCharReplyNameReject     == 0x15);
    REQUIRE(w::kCreateGameReplyInvalidName    == 0x1e);
    REQUIRE(w::kCreateGameReplyNameExist      == 0x1f);
    REQUIRE(w::kCreateGameReplyServerDown     == 0x20);
    REQUIRE(w::kCreateGameReplyNotAvailable   == 0x32);
    REQUIRE(w::kJoinGameReplyBadPass          == 0x29);
    REQUIRE(w::kJoinGameReplyNotExist         == 0x2a);
    REQUIRE(w::kJoinGameReplyGameFull         == 0x2b);
    REQUIRE(w::kJoinGameReplyLevelLimit       == 0x2c);
    REQUIRE(w::kJoinGameReplyHardcoreSoftcore == 0x71);
    REQUIRE(w::kJoinGameReplyNormalNightmare  == 0x73);
    REQUIRE(w::kJoinGameReplyNightmareHell    == 0x74);
    REQUIRE(w::kJoinGameReplyClassicExpansion == 0x78);
    REQUIRE(w::kJoinGameReplyExpansionClassic == 0x79);
    REQUIRE(w::kJoinGameReplyNormalLadder     == 0x7D);
    REQUIRE(w::kCharLoginReplyNotFound        == 0x46);
    REQUIRE(w::kCharLoginReplyExpired         == 0x7b);
}

TEST_CASE("d2cs::wire ladder status flags",
          "[protocol][d2cs][wire_types]")
{
    REQUIRE(w::kLadderStatusDead       == 0x10);
    REQUIRE(w::kLadderStatusHardcore   == 0x20);
    REQUIRE(w::kLadderStatusExpansion  == 0x40);
    REQUIRE(w::kLadderStatusDifficulty == 0x0f00);
}

TEST_CASE("d2cs::wire structs are default-constructed empty",
          "[protocol][d2cs][wire_types]")
{
    w::ClientLoginReq lr{};
    REQUIRE(lr.seqno == 0);
    REQUIRE(lr.secret_hash[0] == 0);
    REQUIRE(lr.secret_hash[4] == 0);

    w::GameInfoReply gi{};
    REQUIRE(gi.charlevel == 0);
    REQUIRE(gi.chclass[0] == 0);
    REQUIRE(gi.chclass[15] == 0);
    REQUIRE(gi.level[15] == 0);

    w::LadderInfo li{};
    REQUIRE(li.explow == 0);
    REQUIRE(li.charname[0] == '\0');
    REQUIRE(li.charname[15] == '\0');
}
