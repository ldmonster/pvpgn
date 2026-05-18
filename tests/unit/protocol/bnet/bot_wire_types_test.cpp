// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/bot_wire_types.hpp"

namespace bot = pvpgn::protocol::bnet::bot;

TEST_CASE("bot_wire_types EID constants match legacy values",
          "[protocol][bnet][bot][wire_types]")
{
    REQUIRE(bot::kEidShowUser            == 1001);
    REQUIRE(bot::kEidJoin                == 1002);
    REQUIRE(bot::kEidLeave               == 1003);
    REQUIRE(bot::kEidWhisper             == 1004);
    REQUIRE(bot::kEidTalk                == 1005);
    REQUIRE(bot::kEidBroadcast           == 1006);
    REQUIRE(bot::kEidChannel             == 1007);
    REQUIRE(bot::kEidUserFlags           == 1009);
    REQUIRE(bot::kEidWhisperSent         == 1010);
    REQUIRE(bot::kEidChannelFull         == 1013);
    REQUIRE(bot::kEidChannelDoesNotExist == 1014);
    REQUIRE(bot::kEidChannelRestricted   == 1015);
}
