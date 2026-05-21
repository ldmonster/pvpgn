// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for pvpgn_v3_observe_file_send and pvpgn_v3_observe_file_raw_send.
///
/// Both bridges are observation-only (always return 0).  Tests verify:
///   1. null conn_ptr  -> returns 0
///   2. null packet_ptr -> returns 0
///   3. valid pointers  -> returns 0 (observation-only, no handler invoked)

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/send_file_bridge.hpp"

namespace {

static int g_conn_dummy   = 0;
static int g_packet_dummy = 0;

static void* CONN   = &g_conn_dummy;
static void* PACKET = &g_packet_dummy;

}  // namespace

// ---------------------------------------------------------------------------
// pvpgn_v3_observe_file_send (packet_class_file site)
// ---------------------------------------------------------------------------

TEST_CASE("pvpgn_v3_observe_file_send: null conn_ptr returns 0", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_send(nullptr, PACKET) == 0);
}

TEST_CASE("pvpgn_v3_observe_file_send: null packet_ptr returns 0", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_send(CONN, nullptr) == 0);
}

TEST_CASE("pvpgn_v3_observe_file_send: both null returns 0", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_send(nullptr, nullptr) == 0);
}

TEST_CASE("pvpgn_v3_observe_file_send: valid pointers returns 0 (observation-only)", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_send(CONN, PACKET) == 0);
}

// ---------------------------------------------------------------------------
// pvpgn_v3_observe_file_raw_send (packet_class_raw site)
// ---------------------------------------------------------------------------

TEST_CASE("pvpgn_v3_observe_file_raw_send: null conn_ptr returns 0", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_raw_send(nullptr, PACKET) == 0);
}

TEST_CASE("pvpgn_v3_observe_file_raw_send: null packet_ptr returns 0", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_raw_send(CONN, nullptr) == 0);
}

TEST_CASE("pvpgn_v3_observe_file_raw_send: both null returns 0", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_raw_send(nullptr, nullptr) == 0);
}

TEST_CASE("pvpgn_v3_observe_file_raw_send: valid pointers returns 0 (observation-only)", "[send_file_bridge]") {
    CHECK(pvpgn_v3_observe_file_raw_send(CONN, PACKET) == 0);
}
