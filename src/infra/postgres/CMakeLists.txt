# SPDX-License-Identifier: GPL-2.0-or-later
# PostgreSQL persistence backend (optional, conditionally compiled)

# Try to find PostgreSQL client library using CMake's built-in finder
find_package(PostgreSQL QUIET)

# Determine whether PostgreSQL support is available
if(PostgreSQL_FOUND)
    set(_pvpgn_postgresql_available TRUE)
    message(STATUS "PostgreSQL client library found — building pvpgn_infra_postgresql with real support")
else()
    set(_pvpgn_postgresql_available FALSE)
    message(STATUS "PostgreSQL client library NOT found — pvpgn_infra_postgresql will be a stub (throws at runtime)")
endif()

# Always define the target so downstream CMakeLists can link against it
# unconditionally; the stub path compiles without any libpq headers.
add_library(pvpgn_infra_postgresql STATIC
    include/infra/postgres/connection.hpp
    include/infra/postgres/account_repository.hpp
    include/infra/postgres/unit_of_work.hpp
    include/infra/postgres/unit_of_work_factory.hpp
    src/connection.cpp
    src/account_repository.cpp
    src/unit_of_work.cpp
    src/unit_of_work_factory.cpp
)

target_include_directories(pvpgn_infra_postgresql
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pvpgn_infra_postgresql
    PUBLIC
        pvpgn_core
        pvpgn_application
        pvpgn_infra_inmemory
)

if(_pvpgn_postgresql_available)
    target_compile_definitions(pvpgn_infra_postgresql PUBLIC PVPGN_V3_WITH_POSTGRESQL)
    target_include_directories(pvpgn_infra_postgresql PRIVATE ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(pvpgn_infra_postgresql PRIVATE ${PostgreSQL_LIBRARIES})
endif()
