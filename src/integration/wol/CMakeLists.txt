# SPDX-License-Identifier: GPL-2.0-or-later
# Integration layer WOL module CMakeLists.txt

add_library(pvpgn_integration_wol)

target_sources(pvpgn_integration_wol
  PRIVATE
    src/wol_session.cpp
)

target_include_directories(pvpgn_integration_wol
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_integration_wol
  PUBLIC
    pvpgn_core
)

pvpgn_v3_apply_flags(pvpgn_integration_wol)
