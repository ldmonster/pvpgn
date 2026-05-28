# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer moderation module CMakeLists.txt
# Defines application_moderation target with moderation use-cases

add_library(pvpgn_application_moderation)

target_sources(pvpgn_application_moderation
  PRIVATE
    src/check_ip_ban.cpp
    src/ban_account.cpp
    src/unban_account.cpp
    src/ban_ip.cpp
    src/kick_connection.cpp
    src/silence_user.cpp
    src/list_bans.cpp
    src/issue_warning.cpp
)

target_include_directories(pvpgn_application_moderation
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_moderation
  PUBLIC
    pvpgn_application_ports
    pvpgn_domain_moderation
    pvpgn_core
)
