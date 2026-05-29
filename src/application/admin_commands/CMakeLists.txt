# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer admin_commands module CMakeLists.txt

add_library(pvpgn_application_admin_commands)

target_sources(pvpgn_application_admin_commands
  PRIVATE
    src/file_help_responder.cpp
    src/help_corpus_parser.cpp
    src/help_corpus.cpp
    src/router.cpp
)

target_include_directories(pvpgn_application_admin_commands
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_admin_commands
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
