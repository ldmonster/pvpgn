# SPDX-License-Identifier: GPL-2.0-or-later
# Unit tests for app::d2dbs — D2DBSSessionHandler
#
# Phase 4 Step 6 — R149

pvpgn_v3_add_test(test_app_d2dbs_session_handler
    SOURCES
        d2dbs_session_handler_test.cpp
    DEPS
        app_d2dbs
        domain_d2dbs
        protocol_d2dbs
)
