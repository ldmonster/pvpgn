# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer profile module CMakeLists.txt

add_library(pvpgn_application_profile)

target_sources(pvpgn_application_profile
  PRIVATE
    src/profile_reply.cpp
)

target_include_directories(pvpgn_application_profile
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_profile
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
