// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for pvpgn_v3_observe_anongame_found.
///
/// This bridge is observation-only (always returns 0) because the v3 anongame
/// encoder does not yet support the complex SERVER_ANONGAME_FOUND packet.
///
/// Covers:
///   1. null conn_ptr -> returns 0
///   2. valid conn_ptr -> returns 0 (observation-only, no handler needed)

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/send_anongame_found_bridge.hpp"

namespace {
static int g_conn_dummy = 0;
static void* CONN = &g_conn_dummy;
}  // namespace

TEST_CASE("observe_anongame_found: null conn returns 0",
          "[send_anongame_found_bridge]") {
    CHECK(pvpgn_v3_observe_anongame_found(nullptr) == 0);
}

TEST_CASE("observe_anongame_found: valid conn returns 0 (observation-only)",
          "[send_anongame_found_bridge]") {
    // Observation bridge always returns 0 regardless of handler state.
    CHECK(pvpgn_v3_observe_anongame_found(CONN) == 0);
}
