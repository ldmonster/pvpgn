add_library(pvpgn_infra_sqlite STATIC
    include/infra/sqlite/connection.hpp
    # Plan 07 (2026-06-02): ALL per-backend SQLite aggregate repos were deleted.
    # They were superseded by the consolidated driver-parameterized repos in
    # src/infra/persistence/ (SQLiteUnitOfWork builds those over a SqliteDriver).
    # This lib is now driver/connection/UoW only — no aggregate repo code.
    include/infra/sqlite/unit_of_work.hpp
    include/infra/sqlite/unit_of_work_factory.hpp
    src/connection.cpp
    src/unit_of_work.cpp
    src/unit_of_work_factory.cpp
    # Plan 07: SQLite IDbDriver adapter (wraps SQLiteConnection as an IDbDriver
    # so the consolidated infra/persistence repositories can run over SQLite).
    ${CMAKE_SOURCE_DIR}/src/infra/persistence/sql_builder/sqlite_driver.cpp
)

target_include_directories(pvpgn_infra_sqlite
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(pvpgn_infra_sqlite
    PUBLIC
        pvpgn_core
        pvpgn_application
        pvpgn_infra_inmemory
        pvpgn_infra_migrations
        sqlite3
)
