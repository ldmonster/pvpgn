# SPDX-License-Identifier: GPL-2.0-or-later
# Application layer anongame_infoply module CMakeLists.txt

add_library(pvpgn_application_anongame_infoply)

target_sources(pvpgn_application_anongame_infoply
  PRIVATE
    src/inforeply_builder.cpp
    src/type_composer.cpp
)

target_include_directories(pvpgn_application_anongame_infoply
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pvpgn_application_anongame_infoply
  PUBLIC
    pvpgn_application_ports
    pvpgn_core
)
