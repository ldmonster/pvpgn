// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/auth_wire_types.hpp"

namespace a = pvpgn::protocol::bnet::auth;

TEST_CASE("auth packet type codes match legacy",
          "[protocol][bnet][auth_wire_types]")
{
    REQUIRE(a::kClientCompInfo1     == 0x05ff);
    REQUIRE(a::kClientCompInfo2     == 0x1eff);
    REQUIRE(a::kServerCompReply     == 0x05ff);
    REQUIRE(a::kServerSessionKey1   == 0x28ff);
    REQUIRE(a::kServerSessionKey2   == 0x1dff);
    REQUIRE(a::kClientCountryInfo1  == 0x12ff);
    REQUIRE(a::kClientAuthInfo      == 0x50ff);
    REQUIRE(a::kClientProgIdent     == 0x06ff);
    REQUIRE(a::kServerAuthReq1      == 0x06ff);
    REQUIRE(a::kServerAuthReq109    == 0x50ff);
    REQUIRE(a::kClientAuthReq1      == 0x07ff);
    REQUIRE(a::kServerAuthReply1    == 0x07ff);
    REQUIRE(a::kServerAuthReply109  == 0x51ff);
    REQUIRE(a::kClientAuthReq109    == 0x51ff);
    REQUIRE(a::kServerRegSnoopReq   == 0x18ff);
    REQUIRE(a::kClientRegSnoopReply == 0x18ff);
    REQUIRE(a::kClientIconReq       == 0x2dff);
    REQUIRE(a::kServerIconReply     == 0x2dff);
}

TEST_CASE("auth CompInfo magic registration fields match legacy",
          "[protocol][bnet][auth_wire_types]")
{
    REQUIRE(a::kCompRegVersion  == 0x00000001);
    REQUIRE(a::kCompRegAuth     == 0xaa8843d1);
    REQUIRE(a::kCompClientId    == 0x001b9dda);
    REQUIRE(a::kCompClientToken == 0xab69f79a);
}

TEST_CASE("auth reply result codes match legacy",
          "[protocol][bnet][auth_wire_types]")
{
    REQUIRE(a::kServerAuthReply1MessageBadVersion == 0x00000000);
    REQUIRE(a::kServerAuthReply1MessageUpdate     == 0x00000001);
    REQUIRE(a::kServerAuthReply1MessageOk         == 0x00000002);

    REQUIRE(a::kServerAuthReply109MessageOk         == 0x00000000);
    REQUIRE(a::kServerAuthReply109MessageUpdate     == 0x00000100);
    REQUIRE(a::kServerAuthReply109MessageBadVersion == 0x00000101);
}

TEST_CASE("auth misc constants match legacy",
          "[protocol][bnet][auth_wire_types]")
{
    REQUIRE(a::kServerSessionKey2Unknown1     == 0x00004df3);
    REQUIRE(a::kServerAuthReq109LogontypeW3   == 0x00000002);
    REQUIRE(a::kServerAuthReq109LogontypeW3xp == 0x00000002);

    REQUIRE(a::kRegSnoopHkeyClassesRoot     == 0x80000000);
    REQUIRE(a::kRegSnoopHkeyCurrentUser     == 0x80000001);
    REQUIRE(a::kRegSnoopHkeyLocalMachine    == 0x80000002);
    REQUIRE(a::kRegSnoopHkeyUsers           == 0x80000003);
    REQUIRE(a::kRegSnoopHkeyPerformanceData == 0x80000004);
    REQUIRE(a::kRegSnoopHkeyCurrentConfig   == 0x80000005);
    REQUIRE(a::kRegSnoopHkeyDynData         == 0x80000006);
    REQUIRE(a::kRegSnoopHkeyPerformanceText == 0x80000050);
    REQUIRE(a::kRegSnoopHkeyPerformanceNlsText == 0x80000060);
}
