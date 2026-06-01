# SPDX-License-Identifier: GPL-2.0-or-later

add_library(infra_persistence OBJECT
    sql_builder/dialect.cpp
)

target_include_directories(infra_persistence
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(infra_persistence
    PUBLIC
        core_error
        core_result
)
