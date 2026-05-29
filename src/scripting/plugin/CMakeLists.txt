cmake_minimum_required(VERSION 3.20)

add_library(pvpgn_scripting_plugin STATIC
    src/semver.cpp
    src/plugin_manifest.cpp
    src/dependency_resolver.cpp
)

target_include_directories(pvpgn_scripting_plugin
    PUBLIC include
)

target_link_libraries(pvpgn_scripting_plugin
    PUBLIC pvpgn_core
)

target_compile_features(pvpgn_scripting_plugin PUBLIC cxx_std_20)
