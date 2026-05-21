// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for the SERVER_UDPTEST observation bridge.
///
/// The bridge is observation-only (always returns 0) because SERVER_UDPTEST
/// is sent via raw UDP (psock_sendto), not conn_push_outqueue.
///
/// Covers:
///   1. null conn_ptr -> returns 0
///   2. valid conn_ptr -> returns 0 (observation-only, no handler needed)

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/send_udptest_bridge.hpp"

namespace {
static int g_conn_dummy = 0;
static void* CONN = &g_conn_dummy;
}  // namespace

// ---------------------------------------------------------------------------
// SERVER_UDPTEST
// ---------------------------------------------------------------------------

TEST_CASE("observe_udptest: null conn returns 0",
          "[send_udptest_bridge]") {
    CHECK(pvpgn_v3_observe_udptest(nullptr) == 0);
}

TEST_CASE("observe_udptest: valid conn returns 0",
          "[send_udptest_bridge]") {
    CHECK(pvpgn_v3_observe_udptest(CONN) == 0);
}
