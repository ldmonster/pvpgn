find_package(Lua 5.4 QUIET)
find_package(Boost 1.82 QUIET COMPONENTS fiber context)

if(LUA_FOUND)
    # Note: sol3 is header-only, so we just need to find it or use FetchContent

    add_library(pvpgn_v3_lua STATIC
        src/lua_host.cpp
        src/sandbox.cpp
        src/plugin_store.cpp
        src/lua_event_bus.cpp
        src/lua_command_registry.cpp
        src/simple_http_client.cpp
        src/fiber_lua_scheduler.cpp
        src/async_lua_db.cpp
    )

    target_include_directories(pvpgn_v3_lua
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
            ${LUA_INCLUDE_DIR}
    )

    target_link_libraries(pvpgn_v3_lua
        PUBLIC
            pvpgn_v3_plugin
            pvpgn_v3_core
            Boost::fiber
            Boost::context
        PRIVATE
            ${LUA_LIBRARIES}
    )

    target_compile_features(pvpgn_v3_lua PRIVATE cxx_std_20)

    # Add sol3 as FetchContent if not already available
    if(NOT TARGET sol3)
        include(FetchContent)
        FetchContent_Declare(sol3
            GIT_REPOSITORY https://github.com/ThePhD/sol2.git
            GIT_TAG v3.3.0
        )
        FetchContent_MakeAvailable(sol3)
        target_link_libraries(pvpgn_v3_lua PUBLIC sol3::sol2)
    endif()
else()
    # Create a stub library when Lua is not available
    add_library(pvpgn_v3_lua INTERFACE)
    target_link_libraries(pvpgn_v3_lua
        INTERFACE
            pvpgn_v3_plugin
            pvpgn_v3_core
    )
endif()
