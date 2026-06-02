# Type utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_types
    STATIC
    SOURCES
        bn_type.cpp
        tag_core.cpp
        tag_clienttag.cpp
        tag_wol.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_DEPS
        core
        core_strings
)
