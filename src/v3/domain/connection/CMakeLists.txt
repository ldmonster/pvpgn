# SPDX-License-Identifier: GPL-2.0-or-later
# ---- domain/connection (ConnectionFsm + IConnectionContext) ----------------
#
# Higher-level domain FSM for a single Battle.net client connection.
# Sits above protocol/bnet/BnetFsm: owns the session lifecycle
# (Connecting → Authenticating → LoggedIn → InChannel → InGame → Disconnecting)
# and dispatches raw BNCS packets to per-state handlers.
#
# Phase 3 Step 2 — R136
# R283: Added pvpgn_application_auth dependency for LoginUserNls (NLS auth).
# R296: Added application_chat dependency for JoinChannel/PostMessage/LeaveChannel.

pvpgn_v3_add_library(domain_connection
    STATIC
    SOURCES
        src/connection_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        domain_shared
        infra_crypto_nls
        application_auth
        application_chat
)
