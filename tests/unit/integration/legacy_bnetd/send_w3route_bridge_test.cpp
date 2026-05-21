// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for the w3route observation bridges.
///
/// All eight w3route bridges are observation-only (always return 0) because
/// the v3 w3route encoder does not yet support these complex packet types.
///
/// Covers (for each of the 8 functions):
///   1. null conn_ptr -> returns 0
///   2. valid conn_ptr -> returns 0 (observation-only, no handler needed)

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/send_w3route_bridge.hpp"

namespace {
static int g_conn_dummy = 0;
static void* CONN = &g_conn_dummy;
}  // namespace

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_ECHOREQ
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_echoreq: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_echoreq(nullptr, 0u) == 0);
}

TEST_CASE("observe_w3route_echoreq: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_echoreq(CONN, 12345u) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_ACK
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_ack: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_ack(nullptr) == 0);
}

TEST_CASE("observe_w3route_ack: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_ack(CONN) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_LOADINGACK
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_loadingack: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_loadingack(nullptr) == 0);
}

TEST_CASE("observe_w3route_loadingack: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_loadingack(CONN) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_READY
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_ready: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_ready(nullptr) == 0);
}

TEST_CASE("observe_w3route_ready: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_ready(CONN) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_PLAYERINFO
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_playerinfo: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_playerinfo(nullptr) == 0);
}

TEST_CASE("observe_w3route_playerinfo: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_playerinfo(CONN) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_LEVELINFO
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_levelinfo: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_levelinfo(nullptr) == 0);
}

TEST_CASE("observe_w3route_levelinfo: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_levelinfo(CONN) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_STARTGAME1
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_startgame1: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_startgame1(nullptr) == 0);
}

TEST_CASE("observe_w3route_startgame1: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_startgame1(CONN) == 0);
}

// ---------------------------------------------------------------------------
// SERVER_W3ROUTE_STARTGAME2
// ---------------------------------------------------------------------------

TEST_CASE("observe_w3route_startgame2: null conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_startgame2(nullptr) == 0);
}

TEST_CASE("observe_w3route_startgame2: valid conn returns 0",
          "[send_w3route_bridge]") {
    CHECK(pvpgn_v3_observe_w3route_startgame2(CONN) == 0);
}
