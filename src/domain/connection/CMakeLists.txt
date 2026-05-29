# SPDX-License-Identifier: GPL-2.0-or-later
# ---- domain/connection (IConnectionContext + RouteRegistry) ---------------
#
# Pure domain primitives for the connection bounded context:
#   - IConnectionContext / GameInfo  (abstract I/O port for the FSM)
#   - RouteRegistry                  (pairing of primary + route conns)
#
# The connection state machine itself lives in `application/connection`
# (it composes application use-cases, which is forbidden in the domain).

pvpgn_v3_add_library(domain_connection
    INTERFACE
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        domain_shared
)
