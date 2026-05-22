# SPDX-License-Identifier: GPL-2.0-or-later
# ---- domain/connection (ConnectionFsm + IConnectionContext) ----------------
#
# Higher-level domain FSM for a single Battle.net client connection.
# Sits above protocol/bnet/BnetFsm: owns the session lifecycle
# (Connecting → Authenticating → LoggedIn → InChannel → InGame → Disconnecting)
# and dispatches raw BNCS packets to per-state handlers.
#
# Phase 3 Step 2 — R136

pvpgn_v3_add_library(domain_connection
    STATIC
    SOURCES
        src/connection_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        domain_shared
)
