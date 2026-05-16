find_package(SQLite3)

if(SQLite3_FOUND)
    pvpgn_v3_add_library(infra_persistence_sqlite
        SOURCES
            src/sqlite_database.cpp
            src/sqlite_account_repository.cpp
        HEADERS
            include/infra/persistence/sqlite/sqlite_database.hpp
            include/infra/persistence/sqlite/sqlite_account_repository.hpp
        DEPS
            pvpgn_v3_application_ports
            pvpgn_v3_domain_identity
            pvpgn_v3_core
            SQLite3::SQLite3
    )
endif()
