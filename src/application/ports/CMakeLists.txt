# SPDX-License-Identifier: GPL-2.0-or-later
# src/application/ports/CMakeLists.txt
#
# application/ports — Header-only library providing port interfaces.
# After Plan 05, ports are defined in domain contexts but re-exported here for compatibility.

pvpgn_v3_add_library(application_ports INTERFACE
    HEADERS
        include/application/ports/ports.hpp
)

target_include_directories(application_ports
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Dependencies
target_link_libraries(application_ports
    INTERFACE
        domain_identity
        domain_connection
        domain_moderation
        domain_chat
        domain_gameplay
        domain_ladder
        domain_matchmaking
        domain_realm
        domain_shared
        domain_social
)
