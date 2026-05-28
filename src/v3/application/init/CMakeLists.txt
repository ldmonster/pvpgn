# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer init module CMakeLists.txt

add_library(pvpgn_application_init)

target_sources(pvpgn_application_init
  PRIVATE
    src/init_conn_dispatch.cpp
)

target_include_directories(pvpgn_application_init
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_init
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
