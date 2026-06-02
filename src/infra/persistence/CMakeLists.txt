# SPDX-License-Identifier: GPL-2.0-or-later

add_library(infra_persistence OBJECT
    sql_builder/dialect.cpp
    src/connection_string.cpp
    repository_factory.cpp
    account_repository.cpp
    channel_repository.cpp
)

target_include_directories(infra_persistence
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(infra_persistence
    PUBLIC
        core
        # Plan 07: the consolidated repositories + factory return domain port
        # interfaces, so the bounded-context port libraries are linked here.
        domain_identity
        domain_social
        domain_gameplay
        domain_ladder
        domain_realm
        domain_moderation
        domain_chat
        domain_shared
)
