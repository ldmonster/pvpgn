// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/d2gs/wire_types.hpp"

namespace w = pvpgn::protocol::d2gs::wire;

TEST_CASE("d2gs::wire constants match legacy",
          "[protocol][d2gs][wire_types]")
{
    REQUIRE(w::kD2csD2gsAuthReq         == 0x10);
    REQUIRE(w::kD2gsD2csAuthReply       == 0x11);
    REQUIRE(w::kD2csD2gsAuthReply       == 0x11);
    REQUIRE(w::kD2gsD2csSetGsInfo       == 0x12);
    REQUIRE(w::kD2csD2gsEchoReq         == 0x13);
    REQUIRE(w::kD2csD2gsControl         == 0x14);
    REQUIRE(w::kD2csD2gsSetInitInfo     == 0x15);
    REQUIRE(w::kD2csD2gsSetConfFile     == 0x16);
    REQUIRE(w::kD2csD2gsCreateGameReq   == 0x20);
    REQUIRE(w::kD2csD2gsJoinGameReq     == 0x21);
    REQUIRE(w::kD2gsD2csUpdateGameInfo  == 0x22);
    REQUIRE(w::kD2gsD2csCloseGame       == 0x23);

    REQUIRE(w::kAuthReplyBadVersion  == 0x01);
    REQUIRE(w::kAuthReplyBadChecksum == 0x02);
    REQUIRE(w::kControlCmdRestart    == 0x01);
    REQUIRE(w::kControlCmdShutdown   == 0x02);

    REQUIRE(w::kDifficultyNormal    == 0);
    REQUIRE(w::kDifficultyNightmare == 1);
    REQUIRE(w::kDifficultyHell      == 2);

    REQUIRE(w::kJoinGameSucceed  == 0);
    REQUIRE(w::kJoinGameGameFull == 2);

    REQUIRE(w::kUpdateGameInfoFlagUpdate == 0);
    REQUIRE(w::kUpdateGameInfoFlagEnter  == 1);
    REQUIRE(w::kUpdateGameInfoFlagLeave  == 2);
}

TEST_CASE("d2gs::wire AuthReplyFromD2gs has 128-byte signature buffer",
          "[protocol][d2gs][wire_types]")
{
    w::AuthReplyFromD2gs r{};
    REQUIRE(r.sign.size() == 128);
    for (auto b : r.sign) REQUIRE(b == 0);
}
