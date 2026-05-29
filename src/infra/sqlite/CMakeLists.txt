add_library(pvpgn_infra_sqlite STATIC
    include/infra/sqlite/connection.hpp
    include/infra/sqlite/account_repository.hpp
    include/infra/sqlite/clan_repository.hpp
    include/infra/sqlite/ladder_repository.hpp
    include/infra/sqlite/ip_ban_repository.hpp
    include/infra/sqlite/account_ban_repository.hpp
    include/infra/sqlite/friend_list_repository.hpp
    include/infra/sqlite/realm_repository.hpp
    include/infra/sqlite/sqlite_channel_repository.hpp
    include/infra/sqlite/unit_of_work.hpp
    include/infra/sqlite/unit_of_work_factory.hpp
    src/connection.cpp
    src/account_repository.cpp
    src/clan_repository.cpp
    src/ladder_repository.cpp
    src/ip_ban_repository.cpp
    src/account_ban_repository.cpp
    src/friend_list_repository.cpp
    src/realm_repository.cpp
    src/sqlite_channel_repository.cpp
    src/unit_of_work.cpp
    src/unit_of_work_factory.cpp
)

target_include_directories(pvpgn_infra_sqlite
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pvpgn_infra_sqlite
    PUBLIC
        pvpgn_core
        pvpgn_application
        pvpgn_infra_inmemory
        pvpgn_infra_migrations
        sqlite3
)
