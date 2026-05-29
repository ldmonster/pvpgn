# SPDX-License-Identifier: GPL-2.0-or-later

# Persistence infrastructure: adapter registry + shared SQL helpers.
#
# `backend_registration.cpp` was excluded because it pulled the (currently
# stale) `pvpgn_infra_sqlite` / `pvpgn_infra_file` headers into every
# consumer of `infra_persistence` and nothing actually invoked the
# registered factories. Re-add it once those adapters compile cleanly.

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
