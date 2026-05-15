# SPDX-License-Identifier: GPL-2.0-or-later
# Infrastructure audit log module CMakeLists.txt

add_library(pvpgn_infra_audit)

target_sources(pvpgn_infra_audit
  PRIVATE
    src/in_memory_audit_log.cpp
)

target_include_directories(pvpgn_infra_audit
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_infra_audit
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
