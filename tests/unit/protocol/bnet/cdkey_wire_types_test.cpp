// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/cdkey_wire_types.hpp"

namespace c = pvpgn::protocol::bnet::cdkey;

TEST_CASE("cdkey packet type codes match legacy",
          "[protocol][bnet][cdkey_wire_types]")
{
    REQUIRE(c::kClientCdkey       == 0x30ff);
    REQUIRE(c::kServerCdkeyReply  == 0x30ff);
    REQUIRE(c::kClientCdkey2      == 0x36ff);
    REQUIRE(c::kServerCdkeyReply2 == 0x36ff);
    REQUIRE(c::kClientCdkey3      == 0x42ff);
    REQUIRE(c::kServerCdkeyReply3 == 0x42ff);
}

TEST_CASE("cdkey reply message codes match legacy",
          "[protocol][bnet][cdkey_wire_types]")
{
    REQUIRE(c::kCdkeyReplyMessageOk       == 0x00000001);
    REQUIRE(c::kCdkeyReplyMessageBad      == 0x00000002);
    REQUIRE(c::kCdkeyReplyMessageWrongApp == 0x00000003);
    REQUIRE(c::kCdkeyReplyMessageError    == 0x00000004);
    REQUIRE(c::kCdkeyReplyMessageInUse    == 0x00000005);
    REQUIRE(c::kCdkeyReply3MessageOk      == 0x00000000);
    REQUIRE(c::kCdkey2SpawnTrue           == 0x00000001);
    REQUIRE(c::kCdkey2SpawnFalse          == 0x00000000);
    REQUIRE(c::kCdkey3Unknown1            == 0xffffffff);
    REQUIRE(c::kCdkey3Unknown6            == 0x00123456);
}
