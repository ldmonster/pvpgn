add_subdirectory(plugin)
add_subdirectory(lua)

# Main scripting library that combines plugin and lua
add_library(pvpgn_v3_scripting INTERFACE)

target_link_libraries(pvpgn_v3_scripting
    INTERFACE
        pvpgn_v3_plugin
        pvpgn_v3_lua
)
