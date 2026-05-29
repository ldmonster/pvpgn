# SPDX-License-Identifier: GPL-2.0-or-later
# ---- application/connection (ConnectionFsm) --------------------------------
#
# Application-layer state machine driving a single BNet client connection.
# Composes auth and chat use-cases; uses `domain::connection::IConnectionContext`
# as its abstract I/O port.
#
# Moved from `domain/connection` (R298) so the FSM can legally depend on
# application use-cases without violating the hexagonal layering rule
# `domain MUST NOT depend on application`.

pvpgn_v3_add_library(application_connection
    STATIC
    SOURCES
        src/connection_fsm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        domain_shared
        domain_connection
        pvpgn_application_auth
        pvpgn_application_chat
)
