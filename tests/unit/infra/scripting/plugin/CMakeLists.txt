# SPDX-License-Identifier: GPL-2.0-or-later

pvpgn_v3_add_test(test_infra_plugin_capability
    SOURCES capability_test.cpp
    DEPS    core infra_scripting_plugin)

# Plan 12: the public plugin ABI capability bits must match the host enum.
pvpgn_v3_add_test(test_infra_plugin_abi_capability_parity
    SOURCES abi_capability_parity_test.cpp
    DEPS    core infra_scripting_plugin)
if(TARGET test_infra_plugin_abi_capability_parity)
    target_include_directories(test_infra_plugin_abi_capability_parity
        PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

pvpgn_v3_add_test(test_infra_plugin_manifest
    SOURCES plugin_manifest_test.cpp
    DEPS    core infra_scripting_plugin)

pvpgn_v3_add_test(test_infra_plugin_loader
    SOURCES plugin_loader_test.cpp
    DEPS    core infra_scripting_plugin)
