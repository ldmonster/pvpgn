# SPDX-License-Identifier: GPL-2.0-or-later

# In-memory persistence implementations
# Thread-safe repositories suitable for tests, development, and CI

pvpgn_v3_add_library(infra_inmemory
    HEADER_ONLY
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC_DEPS
        core
        application_ports
)

# Implementation sources (header-only but includes cpp for factory)
target_sources(infra_inmemory PRIVATE
    src/unit_of_work_factory.cpp
)
