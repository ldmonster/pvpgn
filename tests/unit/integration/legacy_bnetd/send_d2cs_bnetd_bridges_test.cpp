// SPDX-License-Identifier: GPL-2.0-or-later
/// Unit tests for the bnetd→D2CS observation bridges.
///
/// All five bridges are observation-only (always return 0) because the v3
/// D2CS-bnetd link encoder is not yet complete.
///
/// Covers (for each of the 5 functions):
///   1. null conn_ptr -> returns 0
///   2. valid conn_ptr -> returns 0 (observation-only, no handler needed)

#include <catch2/catch_test_macros.hpp>

#include "integration/legacy_bnetd/send_d2cs_bnetd_bridges.hpp"

namespace {
static int g_conn_dummy = 0;
static void* CONN = &g_conn_dummy;
}  // namespace

// ---------------------------------------------------------------------------
// BNETD_D2CS_AUTHREQ
// ---------------------------------------------------------------------------

TEST_CASE("observe_d2cs_bnetd_authreq: null conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_authreq(nullptr, 0u) == 0);
}

TEST_CASE("observe_d2cs_bnetd_authreq: valid conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_authreq(CONN, 42u) == 0);
}

// ---------------------------------------------------------------------------
// BNETD_D2CS_AUTHREPLY
// ---------------------------------------------------------------------------

TEST_CASE("observe_d2cs_bnetd_authreply: null conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_authreply(nullptr, 0u) == 0);
}

TEST_CASE("observe_d2cs_bnetd_authreply: valid conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_authreply(CONN, 1u) == 0);
}

// ---------------------------------------------------------------------------
// BNETD_D2CS_ACCOUNTLOGINREPLY
// ---------------------------------------------------------------------------

TEST_CASE("observe_d2cs_bnetd_accountloginreply: null conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_accountloginreply(nullptr, 0u, 0u) == 0);
}

TEST_CASE("observe_d2cs_bnetd_accountloginreply: valid conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_accountloginreply(CONN, 7u, 1u) == 0);
}

// ---------------------------------------------------------------------------
// BNETD_D2CS_CHARLOGINREPLY
// ---------------------------------------------------------------------------

TEST_CASE("observe_d2cs_bnetd_charloginreply: null conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_charloginreply(nullptr, 0u, 0u) == 0);
}

TEST_CASE("observe_d2cs_bnetd_charloginreply: valid conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_charloginreply(CONN, 3u, 1u) == 0);
}

// ---------------------------------------------------------------------------
// BNETD_D2CS_GAMEINFOREQ
// ---------------------------------------------------------------------------

TEST_CASE("observe_d2cs_bnetd_gameinforeq: null conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_gameinforeq(nullptr, "TestGame") == 0);
}

TEST_CASE("observe_d2cs_bnetd_gameinforeq: valid conn returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_gameinforeq(CONN, "TestGame") == 0);
}

TEST_CASE("observe_d2cs_bnetd_gameinforeq: null gamename returns 0",
          "[send_d2cs_bnetd_bridges]") {
    CHECK(pvpgn_v3_observe_d2cs_bnetd_gameinforeq(CONN, nullptr) == 0);
}
