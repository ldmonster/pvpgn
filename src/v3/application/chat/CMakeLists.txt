# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer chat module CMakeLists.txt

add_library(pvpgn_application_chat)

target_sources(pvpgn_application_chat
  PRIVATE
    src/join_channel.cpp
    src/post_message.cpp
    src/leave_channel.cpp
    src/ban_from_channel.cpp
    src/command_registry.cpp
)

target_include_directories(pvpgn_application_chat
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_chat
  PUBLIC
    pvpgn_application_ports
    pvpgn_domain_chat
    pvpgn_core
)
