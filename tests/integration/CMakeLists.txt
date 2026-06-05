# SPDX-License-Identifier: GPL-2.0-or-later
# Integration tests — require real out-of-process dependencies.
# Gated behind PVPGN_V3_INTEGRATION_TESTS=ON so dev laptops don't pay the cost.

option(PVPGN_V3_INTEGRATION_TESTS "Build and register integration tests" OFF)

if(NOT PVPGN_V3_INTEGRATION_TESTS)
    message(STATUS "Integration tests disabled (PVPGN_V3_INTEGRATION_TESTS=OFF)")
    return()
endif()

# Collect sources
set(_INTEGRATION_SOURCES
    main.cpp
    account_repository_integration_test.cpp
    unit_of_work_integration_test.cpp
    migration_runner_integration_test.cpp
)

add_executable(test_integration ${_INTEGRATION_SOURCES})

target_compile_features(test_integration PRIVATE cxx_std_20)
set_target_properties(test_integration PROPERTIES
    CXX_STANDARD          20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF)

# Link against infra adapters + Catch2
target_link_libraries(test_integration
    PRIVATE
        $<$<TARGET_EXISTS:pvpgn_infra_sqlite>:pvpgn_infra_sqlite>
        $<$<TARGET_EXISTS:pvpgn_infra_mysql>:pvpgn_infra_mysql>
        $<$<TARGET_EXISTS:pvpgn_infra_postgresql>:pvpgn_infra_postgresql>
        $<$<TARGET_EXISTS:pvpgn_infra_migrations>:pvpgn_infra_migrations>
        $<$<TARGET_EXISTS:pvpgn_infra_file>:pvpgn_infra_file>
        pvpgn_application
        domain_identity
        domain_shared
        Catch2::Catch2
)

# The test sources guard backend-specific cases behind PVPGN_HAS_INFRA_* so
# they degrade to a SKIP when an adapter isn't compiled in. Those macros must
# actually be defined when the corresponding target exists, otherwise the
# SQLite/File cases are silently #ifdef'd out and never run. Define them here,
# conditioned on the same targets we (optionally) link above.
if(TARGET pvpgn_infra_sqlite)
    target_compile_definitions(test_integration PRIVATE PVPGN_HAS_INFRA_SQLITE)
endif()
if(TARGET pvpgn_infra_file)
    target_compile_definitions(test_integration PRIVATE PVPGN_HAS_INFRA_FILE)
endif()
if(TARGET pvpgn_infra_mysql)
    target_compile_definitions(test_integration PRIVATE PVPGN_HAS_INFRA_MYSQL)
endif()
if(TARGET pvpgn_infra_postgresql)
    target_compile_definitions(test_integration PRIVATE PVPGN_HAS_INFRA_POSTGRESQL)
endif()

include(CTest)
include(Catch)
catch_discover_tests(test_integration
    TEST_PREFIX "integration."
    PROPERTIES LABELS "integration"
)
