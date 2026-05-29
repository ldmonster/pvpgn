# SPDX-License-Identifier: GPL-2.0-or-later
# Integration layer BNet module CMakeLists.txt

add_library(pvpgn_integration_bnet)

target_sources(pvpgn_integration_bnet
  PRIVATE
    src/bnet_session_handler.cpp
)

target_include_directories(pvpgn_integration_bnet
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_integration_bnet
  PUBLIC
    pvpgn_application_auth
    pvpgn_application_chat
    pvpgn_core
)

pvpgn_v3_apply_flags(pvpgn_integration_bnet)
