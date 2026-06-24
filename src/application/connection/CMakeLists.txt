# SPDX-License-Identifier: GPL-2.0-or-later
# ---- application/connection (ConnectionFsm) --------------------------------
#
# Application-layer state machine driving a single BNet client connection.
# Composes auth and chat use-cases; uses `domain::connection::IConnectionContext`
# as its abstract I/O port.
#
# Lives here (rather than `domain/connection`) so the FSM can legally depend on
# application use-cases without violating the hexagonal layering rule
# `domain MUST NOT depend on application`.

pvpgn_v3_add_library(application_connection
    STATIC
    SOURCES
        # connection_fsm.cpp split by State pattern into focused sub-TUs
        src/connection_fsm.cpp                  # thin coordinator: dispatch() + close() + private helpers
        src/connection_fsm_connecting.cpp        # Connecting state: on_auth_info, on_auth_check, on_logon_request
        src/connection_fsm_authenticating.cpp    # Authenticating state: on_auth_accountlogon, on_auth_accountlogonproof
        src/connection_fsm_loggedin.cpp          # LoggedIn state: on_enter_chat
        src/connection_fsm_inchannel.cpp         # InChannel state: on_join_channel, on_chat_command, on_leave_channel, on_start_game, on_join_game
        src/connection_fsm_ingame.cpp            # InGame state: on_leave_game, on_d2_char_select, on_warcraft_general
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        domain_shared
        domain_connection
        pvpgn_application_auth
        pvpgn_application_chat
)
