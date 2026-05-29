# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer bnet_packet_pump module CMakeLists.txt

add_library(pvpgn_application_bnet_packet_pump)

target_sources(pvpgn_application_bnet_packet_pump
  PRIVATE
    src/lifecycle_placeholder.cpp
)

target_include_directories(pvpgn_application_bnet_packet_pump
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_bnet_packet_pump
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
