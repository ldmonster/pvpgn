# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer ads module CMakeLists.txt

add_library(pvpgn_application_ads)

target_sources(pvpgn_application_ads
  PRIVATE
    src/ad_pick.cpp
)

target_include_directories(pvpgn_application_ads
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_ads
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
