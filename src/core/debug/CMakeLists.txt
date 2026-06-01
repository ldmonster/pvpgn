# Debug utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_debug
    STATIC
    SOURCES
        hexdump.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_DEPS
        core
)
