// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/realm_wire_types.hpp"

namespace r = pvpgn::protocol::bnet::realm;

TEST_CASE("realm packet type codes match legacy",
          "[protocol][bnet][realm_wire_types]")
{
    REQUIRE(r::kClientRealmListReq      == 0x34ff);
    REQUIRE(r::kServerRealmListReply    == 0x34ff);
    REQUIRE(r::kClientRealmListReq110   == 0x40ff);
    REQUIRE(r::kServerRealmListReply110 == 0x40ff);
    REQUIRE(r::kClientRealmJoinReq109   == 0x3eff);
    REQUIRE(r::kServerRealmJoinReply109 == 0x3eff);
}

TEST_CASE("realm payload magic constants match legacy",
          "[protocol][bnet][realm_wire_types]")
{
    REQUIRE(r::kRealmListReplyDataUnknown3 == 0xc0000000);
    REQUIRE(r::kRealmListReplyDataUnknown7 == 0x00018210);
    REQUIRE(r::kRealmListReplyDataUnknown8 == 0xffffffff);
    REQUIRE(r::kRealmListReply110DataUnknown1 == 0x00000001);
}
