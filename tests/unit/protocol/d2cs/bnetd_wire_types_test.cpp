// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/d2cs/bnetd_wire_types.hpp"

namespace bnetd = pvpgn::protocol::d2cs::bnetd;

TEST_CASE("d2cs::bnetd wire constants match legacy",
          "[protocol][d2cs][bnetd][wire_types]")
{
    REQUIRE(bnetd::kBnetdToD2csAuthReq           == 0x01);
    REQUIRE(bnetd::kD2csToBnetdAuthReply         == 0x02);
    REQUIRE(bnetd::kBnetdToD2csAuthReply         == 0x02);
    REQUIRE(bnetd::kD2csToBnetdAccountLoginReq   == 0x10);
    REQUIRE(bnetd::kBnetdToD2csAccountLoginReply == 0x10);
    REQUIRE(bnetd::kD2csToBnetdCharLoginReq      == 0x11);
    REQUIRE(bnetd::kBnetdToD2csCharLoginReply    == 0x11);
    REQUIRE(bnetd::kBnetdToD2csGameInfoReq       == 0x12);
    REQUIRE(bnetd::kD2csToBnetdGameInfoReply     == 0x12);

    REQUIRE(bnetd::kAuthReplySucceed     == 0x00);
    REQUIRE(bnetd::kAuthReplyBadVersion  == 0x01);
    REQUIRE(bnetd::kAccountLoginSucceed  == 0x00);
    REQUIRE(bnetd::kAccountLoginFailed   == 0x01);
    REQUIRE(bnetd::kCharLoginSucceed     == 0x00);
    REQUIRE(bnetd::kCharLoginFailed      == 0x01);
}

TEST_CASE("d2cs::bnetd wire structs default-init cleanly",
          "[protocol][d2cs][bnetd][wire_types]")
{
    bnetd::AccountLoginReq r{};
    REQUIRE(r.h.size       == 0);
    REQUIRE(r.h.type       == 0);
    REQUIRE(r.h.seqno      == 0);
    REQUIRE(r.seqno_inner  == 0);
    REQUIRE(r.sessionnum   == 0);
    REQUIRE(r.sessionkey   == 0);
    for (auto v : r.secret_hash) REQUIRE(v == 0);

    bnetd::GameInfoReply g{};
    REQUIRE(g.difficulty == 0);
}
