# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer email_management module CMakeLists.txt

add_library(pvpgn_application_email_management)

target_sources(pvpgn_application_email_management
  PRIVATE
    src/email_change.cpp
)

target_include_directories(pvpgn_application_email_management
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_email_management
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
