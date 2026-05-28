# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer i18n module CMakeLists.txt

add_library(pvpgn_application_i18n)

target_sources(pvpgn_application_i18n
  PRIVATE
    src/string_table.cpp
)

target_include_directories(pvpgn_application_i18n
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_i18n
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
