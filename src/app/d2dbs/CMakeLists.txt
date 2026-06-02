# SPDX-License-Identifier: GPL-2.0-or-later
# ---- app/d2dbs (D2DBS session handler — Phase 4 Step 6) --------------------
#
# `app_d2dbs` is a static library that bridges the D2DBS protocol FSM
# (`protocol_d2dbs`) to the domain use cases (`domain_d2dbs`).
#
# It contains:
#   - `D2DBSSessionHandler`  — translates D2DBSFsmCallbacks to use-case calls
#   - `ID2DBSSessionEgress`  — pure-virtual outbound port (header-only)
#
# Phase 4 Step 6 — R149

pvpgn_v3_add_library(app_d2dbs
    STATIC
    SOURCES
        src/d2dbs_session_handler.cpp
        # MIGRATED from legacy_d2dbs/src/ (Plan 04 strangler)
        src/send_packet_bridge.cpp
        src/send_echorequest_bridge.cpp
        src/d2dbs_prefs_bridge.cpp
        src/bridge_logger.cpp
        src/charlock_bridge.cpp
        src/d2ladder_bridge.cpp
        src/dbsdupecheck_bridge.cpp
        src/dbserver_bridge.cpp
        src/handle_signal_bridge.cpp
        src/dbspacket_bridge.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        protocol_d2dbs
        domain_d2dbs
        infra_config
)
