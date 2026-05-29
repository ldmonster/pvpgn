# SPDX-License-Identifier: GPL-2.0-or-later
# plugins/example-quiz/native/CMakeLists.txt
#
# Builds the example-quiz native plugin as a shared library.
# The resulting .so/.dll can be placed in the server's native plugin directory.

cmake_minimum_required(VERSION 3.20)
project(example_quiz_plugin C)

# Build as a shared library (no "lib" prefix on Linux for clarity).
add_library(example_quiz_plugin SHARED main.c)
set_target_properties(example_quiz_plugin PROPERTIES PREFIX "")

# Point at the pvpgn plugin C ABI header.
# When built as part of the main tree, PVPGN_PLUGIN_INCLUDE_DIR can be set
# by the parent CMakeLists; otherwise fall back to a relative path.
if(NOT DEFINED PVPGN_PLUGIN_INCLUDE_DIR)
    set(PVPGN_PLUGIN_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/infra/plugin/include")
endif()

target_include_directories(example_quiz_plugin
    PRIVATE ${PVPGN_PLUGIN_INCLUDE_DIR}
)

# C99 is sufficient for the plugin ABI.
target_compile_features(example_quiz_plugin PRIVATE c_std_99)

# Install alongside other plugins.
install(TARGETS example_quiz_plugin
    LIBRARY DESTINATION plugins/native
    RUNTIME DESTINATION plugins/native   # Windows DLL
)
