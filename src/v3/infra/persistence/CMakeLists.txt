# SPDX-License-Identifier: GPL-2.0-or-later

# Persistence infrastructure: adapter registry and backend factories

pvpgn_v3_add_library(infra_persistence
    SOURCES
        src/adapter_registry.cpp
        src/backend_registration.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
        infra_sqlite
        infra_file
)
