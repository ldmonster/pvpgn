# String utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_strings
    STATIC
    SOURCES
        xstring.cpp
        util_string.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/common
    PUBLIC_DEPS
        core
)
