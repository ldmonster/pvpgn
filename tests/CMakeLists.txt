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
