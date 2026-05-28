# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer game module CMakeLists.txt
# Defines application_game target with game use-cases

add_library(pvpgn_application_game)

target_sources(pvpgn_application_game
  PRIVATE
    src/start_game.cpp
    src/join_game.cpp
    src/leave_game.cpp
    src/cancel_game.cpp
    src/report_game_result.cpp
    src/list_public_games.cpp
    src/create_private_game.cpp
)

target_include_directories(pvpgn_application_game
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_game
  PUBLIC
    pvpgn_application_ports
    pvpgn_domain_gameplay
    pvpgn_core
)
