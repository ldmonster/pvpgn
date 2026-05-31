# SPDX-License-Identifier: GPL-2.0-or-later
# Catch2 v3 conformance tests for the plugin and scripting subsystem (Plan 11)

# The scripting tests depend on pvpgn_scripting_plugin / pvpgn_infra_scripting
# which only exist when Lua is enabled (see src/infra/scripting/CMakeLists.txt).
if(NOT PVPGN_V3_WITH_LUA)
    return()
endif()

# Existing GTest-based plugin unit tests
add_subdirectory(plugin)

if(PVPGN_V3_PLUGIN_TESTS)
    # ── 11.1  Plugin ABI conformance tests ──────────────────────────────────
    add_executable(test_plugin_abi_conformance
        plugin_abi_conformance_test.cpp
    )
    target_link_libraries(test_plugin_abi_conformance
        PRIVATE
            pvpgn_scripting_plugin
            Catch2::Catch2WithMain
    )
    target_compile_features(test_plugin_abi_conformance PRIVATE cxx_std_20)
    add_test(NAME PluginABIConformance COMMAND test_plugin_abi_conformance)

    # ── 11.2  Lua API v2 interface tests ────────────────────────────────────
    add_executable(test_lua_api_v2
        lua_api_v2_test.cpp
    )
    target_link_libraries(test_lua_api_v2
        PRIVATE
            pvpgn_scripting_plugin
            Catch2::Catch2WithMain
    )
    target_compile_features(test_lua_api_v2 PRIVATE cxx_std_20)
    # Workaround for gcc 15 ICE during RTL combine pass on this TU
    # (segfault in run_fast_df_dce); disable combine/dce with -O0.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(test_lua_api_v2 PRIVATE -O0 -fno-dce -fno-tree-dce)
    endif()
    add_test(NAME LuaAPIv2 COMMAND test_lua_api_v2)
endif()
