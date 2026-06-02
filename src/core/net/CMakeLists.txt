# Network address utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_net
    STATIC
    SOURCES
        addr_core.cpp
        addr_list.cpp
        addr_netaddr.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE_INCLUDES
        ${CMAKE_SOURCE_DIR}/src/core/util
    PUBLIC_DEPS
        core
        core_strings
)
