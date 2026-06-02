// SPDX-License-Identifier: GPL-2.0-or-later
/// @file send_outbound_obs_bridges_test.cpp
/// Unit tests for the D2CS outbound observation bridges.
/// All bridges are no-ops (return 0); tests verify the ABI is callable
/// and that null conn_ptr is handled gracefully.

#include "app/d2cs/legacy_d2cs_bridges/send_outbound_obs_bridges.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("pvpgn_v3_d2cs_obs_accountloginreq_bnetd returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_accountloginreq_bnetd(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_accountloginreq_bnetd(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_charloginreq_bnetd returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_charloginreq_bnetd(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_charloginreq_bnetd(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_creategamereq_d2gs returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_creategamereq_d2gs(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_creategamereq_d2gs(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_joingamereq_d2gs returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_joingamereq_d2gs(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_joingamereq_d2gs(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_echoreq_d2gs returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_echoreq_d2gs(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_echoreq_d2gs(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_control_d2gs returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_control_d2gs(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_control_d2gs(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_ladderreply returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_ladderreply(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_ladderreply(reinterpret_cast<void*>(0x1)) == 0);
}

TEST_CASE("pvpgn_v3_d2cs_obs_charlistreply returns 0", "[d2cs][obs]") {
    CHECK(pvpgn_v3_d2cs_obs_charlistreply(nullptr) == 0);
    CHECK(pvpgn_v3_d2cs_obs_charlistreply(reinterpret_cast<void*>(0x1)) == 0);
}
