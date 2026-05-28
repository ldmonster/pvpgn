# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer icon_table module CMakeLists.txt

add_library(pvpgn_application_icon_table)

target_sources(pvpgn_application_icon_table
  PRIVATE
    src/icon_table.cpp
)

target_include_directories(pvpgn_application_icon_table
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_icon_table
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
