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

# Consume the sqlite3 header/library discovered (or overridden) in
# src/CMakeLists.txt via find_path/find_library. Using the resolved paths
# rather than a bare `sqlite3` makes the backend build correctly whether
# sqlite lives on the default system paths or in a non-standard prefix.
target_include_directories(pvpgn_infra_sqlite SYSTEM PRIVATE
    ${PVPGN_V3_SQLITE3_INCLUDE_DIR}
)

target_link_libraries(pvpgn_infra_sqlite
    PUBLIC
        pvpgn_core
        pvpgn_application
        pvpgn_infra_inmemory
        pvpgn_infra_migrations
        # unit_of_work.cpp builds the consolidated Sql*Repository classes over a
        # SqliteDriver; their vtables/definitions live in infra_persistence, so
        # the backend must link it (no cycle — infra_persistence links only
        # core + domain port libs).
        infra_persistence
        ${PVPGN_V3_SQLITE3_LIBRARY}
)
