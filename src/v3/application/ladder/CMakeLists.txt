# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer ladder module CMakeLists.txt
# Defines pvpgn_application_ladder target with ladder use-cases

add_library(pvpgn_application_ladder)

target_sources(pvpgn_application_ladder
  PRIVATE
    src/recompute_ladder.cpp
    src/get_ladder_entry.cpp
    src/get_ladder_page.cpp
)

target_include_directories(pvpgn_application_ladder
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_ladder
  PUBLIC
    pvpgn_application_ports
    pvpgn_v3_core
)
