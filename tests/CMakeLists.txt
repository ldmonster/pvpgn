# SPDX-License-Identifier: GPL-2.0-or-later
# PvPGN Test Suite — Test Pyramid
#
# Layers (from fastest/most isolated to slowest/most integrated):
#   unit/        — per-class/per-function; no I/O, no network, no Lua
#   functional/  — single use-case with InMemory infra adapters
#   integration/ — real infra adapters (SQLite, Asio loopback); opt-in
#   e2e/         — real bnetd + real client tools; opt-in
#   fuzz/        — libFuzzer targets; opt-in
#
# CMake options:
#   PVPGN_V3_BUILD_TESTS=ON         (default: ON in dev/asan/tsan/coverage presets)
#   PVPGN_V3_INTEGRATION_TESTS=ON   (default: OFF; requires real infra)
#   PVPGN_V3_E2E_TESTS=ON           (default: OFF; requires running bnetd)
#   PVPGN_V3_FUZZ_TESTS=ON          (default: OFF; requires clang + libFuzzer)

if(NOT PVPGN_V3_BUILD_TESTS)
    return()
endif()

# Re-issue find_package() for the libraries that test targets link to
# directly (e.g. Boost::system, OpenSSL::SSL, Threads::Threads). These
# were originally found inside src/CMakeLists.txt; when the test subtree
# lived under src/ those IMPORTED targets were visible to test code via
# directory inheritance. Now that tests/ is registered from the top-level
# CMakeLists.txt as a sibling of src/, we must surface them here too.
# find_package() is idempotent and cheap when the result is already cached.
find_package(Boost 1.75 QUIET COMPONENTS system)
find_package(OpenSSL QUIET)
find_package(Threads QUIET)
find_package(ZLIB QUIET)

# ---- Catch2 (shared by all test subdirectories) ----------------------------
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# Ensure Catch2 is built with C++20 to support std::string_view
foreach(_t Catch2 Catch2WithMain)
    if(TARGET ${_t})
        target_compile_features(${_t} PUBLIC cxx_std_20)
        set_target_properties(${_t} PROPERTIES SYSTEM TRUE)
    endif()
endforeach()

include(CTest)

add_subdirectory(unit)
add_subdirectory(functional)

if(PVPGN_V3_INTEGRATION_TESTS)
    add_subdirectory(integration)
endif()

if(PVPGN_V3_E2E_TESTS)
    add_subdirectory(e2e)
endif()

if(PVPGN_V3_FUZZ_TESTS)
    add_subdirectory(fuzz)
endif()
