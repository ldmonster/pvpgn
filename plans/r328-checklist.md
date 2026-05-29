# R328 Checklist — MySQL Adapter Real Implementation

## Goal
Create a real MySQL-backed `IUnitOfWorkFactory` and `IAccountRepository` in
`src/v3/infra/mysql/`. If `libmysqlclient` / `mariadb-connector-c` is not found
at CMake time, the target still builds but throws `std::runtime_error` at runtime.

## Tasks

- [x] Read port interfaces (`IAccountRepository`, `IUnitOfWork`, `IUnitOfWorkFactory`)
- [x] Read SQLite reference implementation for patterns
- [x] Read existing MySQL stub headers (`connection.hpp`, `account_repository.hpp`,
      `unit_of_work.hpp`, `unit_of_work_factory.hpp`)
- [x] Create `src/v3/infra/mysql/src/connection.cpp`
      — Full MySQL C API implementation (`mysql_init`, `mysql_real_connect`,
        `mysql_real_query`, `mysql_store_result`, `mysql_fetch_row`)
      — Guarded by `#ifdef PVPGN_V3_WITH_MYSQL`
- [x] Create `src/v3/infra/mysql/src/account_repository.cpp`
      — `find_by_id`, `find_by_name`, `save` (INSERT … ON DUPLICATE KEY UPDATE),
        `remove`, `forEach`, `size`
      — Helpers: `bn_hash_from_hex`, `bn_hash_to_hex`, `account_from_row`, `sql_escape`
- [x] Update `src/v3/infra/mysql/include/infra/mysql/unit_of_work.hpp`
      — Added all in-memory fallback repo members and includes
- [x] Create `src/v3/infra/mysql/src/unit_of_work.cpp`
      — Accounts → MySQL; channels/games/clans/etc. → in-memory
- [x] Create `src/v3/infra/mysql/src/unit_of_work_factory.cpp`
      — Parses `"host:port:user:pass:db"` connection string
      — Creates `MySQLConnection` + `MySQLUnitOfWork` per `create()` call
- [x] Update `src/v3/infra/mysql/CMakeLists.txt`
      — Changed from INTERFACE stub to STATIC library (`pvpgn_infra_mysql`)
      — `find_package(MySQL QUIET)` with pkg-config fallback for `mysqlclient`/`mariadb`
      — `PVPGN_V3_WITH_MYSQL` compile definition when library found
      — Links `pvpgn_core`, `pvpgn_application`, `pvpgn_infra_inmemory`
- [x] Add `add_subdirectory(infra/mysql)` to `src/v3/CMakeLists.txt`

## Files Changed
| File | Change |
|------|--------|
| `src/v3/infra/mysql/src/connection.cpp` | Created |
| `src/v3/infra/mysql/src/account_repository.cpp` | Created |
| `src/v3/infra/mysql/src/unit_of_work.cpp` | Created |
| `src/v3/infra/mysql/src/unit_of_work_factory.cpp` | Created |
| `src/v3/infra/mysql/include/infra/mysql/unit_of_work.hpp` | Modified |
| `src/v3/infra/mysql/CMakeLists.txt` | Modified |
| `src/v3/CMakeLists.txt` | Modified (add_subdirectory) |

## Notes
- Connection string format: `"host:port:user:password:database"`
  e.g. `"127.0.0.1:3306:pvpgn:secret:pvpgn_db"`
- SQL upsert: `INSERT INTO accounts … ON DUPLICATE KEY UPDATE …`
- `void*` opaque handles in headers — no MySQL headers in public API
- Non-account repos use in-memory fallbacks (session-scoped data)
