# SPDX-License-Identifier: GPL-2.0-or-later
# Legacy GTest-based plugin unit tests. Skipped silently when GTest is
# not available so the rest of the test tree can still configure on
# minimal toolchains.
find_package(GTest QUIET)
if(NOT TARGET GTest::gtest_main)
    message(STATUS "tests/unit/scripting/plugin: GTest not found -- skipping")
    return()
endif()

add_executable(test_semver semver_test.cpp)
target_link_libraries(test_semver PRIVATE pvpgn_scripting_plugin GTest::gtest_main)
add_test(NAME SemVerTest COMMAND test_semver)

add_executable(test_dependency_resolver dependency_resolver_test.cpp)
target_link_libraries(test_dependency_resolver PRIVATE pvpgn_scripting_plugin GTest::gtest_main)
add_test(NAME DependencyResolverTest COMMAND test_dependency_resolver)
