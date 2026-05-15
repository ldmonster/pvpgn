# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer auth module CMakeLists.txt

add_library(pvpgn_application_auth)

target_sources(pvpgn_application_auth
  PRIVATE
    src/login_user.cpp
    src/permission_checker.cpp
)

target_include_directories(pvpgn_application_auth
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_auth
  PUBLIC
    pvpgn_application_ports
    pvpgn_domain_identity
    pvpgn_core
)
