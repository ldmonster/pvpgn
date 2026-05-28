# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer anongame_lobby module CMakeLists.txt

add_library(pvpgn_application_anongame_lobby)

target_sources(pvpgn_application_anongame_lobby
  PRIVATE
    src/lobby.cpp
)

target_include_directories(pvpgn_application_anongame_lobby
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_anongame_lobby
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
