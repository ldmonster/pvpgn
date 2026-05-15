# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(test_infra_inmemory
    in_memory_unit_of_work_test.cpp
    in_memory_clan_repository_test.cpp
    in_memory_ladder_repository_test.cpp
)

target_link_libraries(test_infra_inmemory
    gtest
    gtest_main
    pvpgn_infra_inmemory
    pvpgn_application_ports
)

target_include_directories(test_infra_inmemory
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/v3
)

add_test(NAME InMemoryInfraTests COMMAND test_infra_inmemory)
