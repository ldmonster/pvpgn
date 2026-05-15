# SPDX-License-Identifier: GPL-2.0-or-later
# PostgreSQL persistence backend (optional)

option(PVPGN_V3_WITH_POSTGRESQL "Build with PostgreSQL support" OFF)

if(PVPGN_V3_WITH_POSTGRESQL)
    # PostgreSQL header-only include for now (stub implementations)
    add_library(infra_postgres INTERFACE)
    target_include_directories(infra_postgres
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # Link dependencies
    target_link_libraries(infra_postgres
        INTERFACE
        core
        application_ports
    )

    # TODO: Find PostgreSQL client library
    # find_package(PostgreSQL REQUIRED)
    # target_link_libraries(infra_postgres INTERFACE PostgreSQL::PostgreSQL)

    # Export the target
    set(PVPGN_V3_POSTGRES_FOUND TRUE PARENT_SCOPE)
endif()
