// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/misc_wire_types.hpp"

namespace m = pvpgn::protocol::bnet::misc;

TEST_CASE("misc ad/echo/ping packet codes match legacy",
          "[protocol][bnet][misc_wire_types]")
{
    REQUIRE(m::kClientAdReq         == 0x15ff);
    REQUIRE(m::kServerAdReply       == 0x15ff);
    REQUIRE(m::kClientAdAck         == 0x21ff);
    REQUIRE(m::kClientAdClick       == 0x16ff);
    REQUIRE(m::kClientAdClick2      == 0x41ff);
    REQUIRE(m::kServerAdClickReply2 == 0x41ff);

    REQUIRE(m::kClientEchoReply == 0x25ff);
    REQUIRE(m::kServerEchoReq   == 0x25ff);
    REQUIRE(m::kClientPingReq   == 0x00ff);
    REQUIRE(m::kServerPingReply == 0x00ff);
}

TEST_CASE("misc file-info constants match legacy",
          "[protocol][bnet][misc_wire_types]")
{
    REQUIRE(m::kClientFileInfoReq   == 0x33ff);
    REQUIRE(m::kServerFileInfoReply == 0x33ff);

    REQUIRE(m::kFileInfoReqTypeTos        == 0x0000001a);
    REQUIRE(m::kFileInfoReqTypeGateways   == 0x0000001b);
    REQUIRE(m::kFileInfoReqTypeGatewaysD2 == 0x80000004);
    REQUIRE(m::kFileInfoReqTypeIcons      == 0x0000001d);

    REQUIRE(m::kFileInfoReplyTypeExtraWork == 0x80000005);
}

TEST_CASE("misc motd / readmemory / unknown_24 codes match legacy",
          "[protocol][bnet][misc_wire_types]")
{
    REQUIRE(m::kClientMotdW3        == 0x46ff);
    REQUIRE(m::kServerMotdW3        == 0x46ff);
    REQUIRE(m::kClientMotdReq       == 0x46ff);
    REQUIRE(m::kServerMotdW3MsgType == 0x01);
    REQUIRE(m::kServerMotdW3Welcome == 0x00000000);

    REQUIRE(m::kClientReadMemory == 0x17ff);
    REQUIRE(m::kServerReadMemory == 0x17ff);

    REQUIRE(m::kClientUnknown24 == 0x24ff);
}

TEST_CASE("misc message-box / required-work / extra-work codes match legacy",
          "[protocol][bnet][misc_wire_types]")
{
    REQUIRE(m::kServerMessageBox  == 0x19ff);
    REQUIRE(m::kMessageBoxOk       == 0x00000000);
    REQUIRE(m::kMessageBoxOkCancel == 0x00000001);
    REQUIRE(m::kMessageBoxYesNo    == 0x00000004);

    REQUIRE(m::kServerRequiredWork == 0x4Cff);
    REQUIRE(m::kClientExtraWork    == 0x4bff);
    REQUIRE(m::kClientChangeClient == 0x5cff);
    REQUIRE(m::kClientCrashDump    == 0x5dff);
}
