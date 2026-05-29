# SPDX-License-Identifier: GPL-2.0-or-later

# Persistence infrastructure: adapter registry, shared SQL helpers, and
# backend registration glue (SQLite + File).
#
# NOTE: backend_registration.cpp includes headers from pvpgn_infra_sqlite
# and pvpgn_infra_file. Those targets are added by src/v3/CMakeLists.txt
# *after* this subdirectory, so the link dependencies are wired there
# (see the block immediately after add_subdirectory(infra/sqlite) etc.).

pvpgn_v3_add_library(infra_persistence
    STATIC
    SOURCES
        src/adapter_registry.cpp
        src/connection_string.cpp
        src/backend_registration.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
)
