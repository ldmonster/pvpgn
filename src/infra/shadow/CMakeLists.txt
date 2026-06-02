# SPDX-License-Identifier: GPL-2.0-or-later

add_library(pvpgn_infra_shadow STATIC
    include/infra/shadow/shadow_account_repository.hpp
    include/infra/shadow/shadow_unit_of_work.hpp
    include/infra/shadow/shadow_unit_of_work_factory.hpp
    src/shadow_account_repository.cpp
    src/shadow_unit_of_work.cpp
    src/shadow_unit_of_work_factory.cpp
)

target_include_directories(pvpgn_infra_shadow
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/application/persistence/include
)

target_link_libraries(pvpgn_infra_shadow
    PUBLIC
        pvpgn_core
        pvpgn_application
)
