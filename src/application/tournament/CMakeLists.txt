# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer tournament module CMakeLists.txt

add_library(pvpgn_application_tournament)

target_sources(pvpgn_application_tournament
  PRIVATE
    src/tournament_reply.cpp
)

target_include_directories(pvpgn_application_tournament
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_tournament
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
