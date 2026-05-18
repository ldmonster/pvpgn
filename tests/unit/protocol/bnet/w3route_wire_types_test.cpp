// SPDX-License-Identifier: GPL-2.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "protocol/bnet/w3route_wire_types.hpp"

namespace w = pvpgn::protocol::bnet::w3route;

TEST_CASE("w3route packet type codes match legacy",
          "[protocol][bnet][w3route_wire_types]")
{
    REQUIRE(w::kClientReq            == 0x1ef7);
    REQUIRE(w::kClientLoadingDone    == 0x23f7);
    REQUIRE(w::kServerReady          == 0x14f7);
    REQUIRE(w::kClientAbort          == 0x21f7);
    REQUIRE(w::kServerLoadingAck     == 0x08f7);
    REQUIRE(w::kClientConnected      == 0x3bf7);
    REQUIRE(w::kServerEchoReq        == 0x01f7);
    REQUIRE(w::kClientEchoReply      == 0x46f7);
    REQUIRE(w::kClientGameResult     == 0x2ef7);
    REQUIRE(w::kClientGameResultW3xp == 0x3af7);
    REQUIRE(w::kServerAck            == 0x04f7);
    REQUIRE(w::kServerPlayerInfo     == 0x06f7);
    REQUIRE(w::kServerLevelInfo      == 0x47f7);
    REQUIRE(w::kServerStartGame1     == 0x0af7);
    REQUIRE(w::kServerStartGame2     == 0x0bf7);
}

TEST_CASE("w3route per-player game result codes match legacy",
          "[protocol][bnet][w3route_wire_types]")
{
    REQUIRE(w::kGameResultLoss == 0x00000003);
    REQUIRE(w::kGameResultWin  == 0x00000004);
}

TEST_CASE("w3route server-ack magic constant matches legacy",
          "[protocol][bnet][w3route_wire_types]")
{
    REQUIRE(w::kServerAckUnknown3 == 0x484e2637);
}
