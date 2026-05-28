# SPDX-License-Identifier: GPL-2.0-or-later

add_library(pvpgn_application_social)

target_sources(pvpgn_application_social
  PRIVATE
    src/add_friend.cpp
    src/remove_friend.cpp
    src/list_friends.cpp
    src/create_clan.cpp
    src/disband_clan.cpp
    src/invite_to_clan.cpp
    src/kick_from_clan.cpp
    src/promote_clan_member.cpp
    src/set_clan_motd.cpp
    src/join_clan.cpp
    src/leave_clan.cpp
    src/create_team.cpp
    src/disband_team.cpp
)

target_include_directories(pvpgn_application_social
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_social
  PUBLIC
    pvpgn_application_ports
    pvpgn_domain_social
    pvpgn_core
)
