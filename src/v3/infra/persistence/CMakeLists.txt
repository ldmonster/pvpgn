# SPDX-License-Identifier: GPL-2.0-or-later

# Persistence infrastructure: adapter registry and shared SQL helpers.
#
# This library currently depends only on `core` and `application_ports`.
# The backend registration glue (backend_registration.cpp) and the
# realm / sqlite sub-directories still have stale target-name
# references (`infra_sqlite` vs `pvpgn_infra_sqlite`) that must be
# cleaned up before they can be linked, so they are intentionally not
# built here yet.

pvpgn_v3_add_library(infra_persistence
    STATIC
    SOURCES
        src/adapter_registry.cpp
        src/connection_string.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
)
