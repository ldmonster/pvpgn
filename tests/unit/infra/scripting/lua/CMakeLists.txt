if(LUA_FOUND AND GTEST_FOUND)
    add_executable(lua_api_test
        api_test.cpp
    )

    target_include_directories(lua_api_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../..
            ${LUA_INCLUDE_DIR}
    )

    target_link_libraries(lua_api_test
        PRIVATE
            pvpgn_v3_lua
            pvpgn_v3_plugin
            pvpgn_v3_core
            gtest
            gtest_main
    )

    target_compile_features(lua_api_test PRIVATE cxx_std_20)

    add_test(
        NAME lua_api_test
        COMMAND lua_api_test
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

if(LUA_FOUND AND GTEST_FOUND)
    find_package(Boost 1.82 QUIET COMPONENTS fiber context)
    
    if(Boost_FOUND)
        add_executable(fiber_lua_scheduler_test
            fiber_lua_scheduler_test.cpp
        )

        target_include_directories(fiber_lua_scheduler_test
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/../../../..
                ${LUA_INCLUDE_DIR}
        )

        target_link_libraries(fiber_lua_scheduler_test
            PRIVATE
                pvpgn_v3_lua
                pvpgn_v3_core
                gtest
                gtest_main
                Boost::fiber
                Boost::context
        )

        target_compile_features(fiber_lua_scheduler_test PRIVATE cxx_std_20)

        add_test(
            NAME fiber_lua_scheduler_test
            COMMAND fiber_lua_scheduler_test
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    endif()
endif()
