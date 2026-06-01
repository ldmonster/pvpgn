# Time utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_time
    STATIC
    SOURCES
        bnettime.cpp
        util_time.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/common
    PUBLIC_DEPS
        core
)
