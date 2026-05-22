# SPDX-License-Identifier: GPL-2.0-or-later
# Unit tests for app::d2cs — D2CSSessionHandler
#
# Phase 4 Step 3 — R146

pvpgn_v3_add_test(test_app_d2cs_session_handler
    SOURCES
        d2cs_session_handler_test.cpp
    DEPS
        app_d2cs
        domain_d2cs
        protocol_d2cs
)
