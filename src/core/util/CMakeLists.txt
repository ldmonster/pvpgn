# General utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_util
    STATIC
    SOURCES
        util.cpp
        util_file.cpp
        token.cpp
        trans.cpp
        proginfo.cpp
        peerchat.cpp
        rcm.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_DEPS
        core
)
