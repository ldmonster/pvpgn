# Network address utilities (moved from src/common/ in Plan 02)
pvpgn_v3_add_library(core_net
    STATIC
    SOURCES
        addr.cpp
        addr_core.cpp
        addr_list.cpp
        addr_netaddr.cpp
    PUBLIC_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC_DEPS
        core
)
