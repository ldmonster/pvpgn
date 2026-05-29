# Fetch sol2 if not already available
# DISABLED: sol2 requires Lua development headers which are not installed
# if(NOT TARGET sol2::sol2)
#     include(FetchContent)
#     FetchContent_Declare(sol2
#         GIT_REPOSITORY https://github.com/ThePhD/sol2.git
#         GIT_TAG v3.3.0
#     )
#     FetchContent_MakeAvailable(sol2)
# endif()

add_library(pvpgn_v3_plugin STATIC
    src/capability.cpp
    src/plugin_manifest.cpp
    src/plugin_loader.cpp
)

target_include_directories(pvpgn_v3_plugin
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../lua/include
)

target_link_libraries(pvpgn_v3_plugin
    PUBLIC
        core
        # sol2::sol2  # DISABLED: sol2 requires Lua development headers
    PRIVATE
        dl
)

target_compile_features(pvpgn_v3_plugin PRIVATE cxx_std_20)
