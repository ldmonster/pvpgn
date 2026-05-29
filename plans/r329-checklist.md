# R329 Checklist — PostgreSQL Adapter Real Implementation

## Goal
Create a real PostgreSQL-backed `IUnitOfWorkFactory` and `IAccountRepository` in
`src/v3/infra/postgres/`. If `libpq` is not found at CMake time, the target still
builds but throws `std::runtime_error` at runtime.

## Tasks

- [x] Read port interfaces (`IAccountRepository`, `IUnitOfWork`, `IUnitOfWorkFactory`)
- [x] Read SQLite reference implementation for patterns
- [x] Read existing PostgreSQL stub headers (`connection.hpp`, `account_repository.hpp`,
      `unit_of_work.hpp`, `unit_of_work_factory.hpp`)
- [x] Create `src/v3/infra/postgres/src/connection.cpp`
      — Full libpq implementation (`PQconnectdb`, `PQexec`, `PQgetvalue`, `PQntuples`)
      — `Row` uses `PGresult*` opaque handle with `row_num_` index
      — `begin`/`commit`/`rollback` via `PQexec("BEGIN/COMMIT/ROLLBACK")`
      — Guarded by `#ifdef PVPGN_V3_WITH_POSTGRESQL`
- [x] Create `src/v3/infra/postgres/src/account_repository.cpp`
      — `find_by_id`, `find_by_name`, `save` (INSERT … ON CONFLICT DO UPDATE),
        `remove`, `forEach`, `size`
      — Helpers: `bn_hash_from_hex`, `bn_hash_to_hex`, `account_from_row`, `sql_escape`
      — PostgreSQL uses `ON CONFLICT (id) DO UPDATE SET …` for upsert
- [x] Update `src/v3/infra/postgres/include/infra/postgres/unit_of_work.hpp`
      — Added all in-memory fallback repo members and includes
- [x] Create `src/v3/infra/postgres/src/unit_of_work.cpp`
      — Accounts → PostgreSQL; channels/games/clans/etc. → in-memory
- [x] Create `src/v3/infra/postgres/src/unit_of_work_factory.cpp`
      — Parses `"host:port:user:pass:db"` connection string
      — Creates `PostgreSQLConnection` + `PostgreSQLUnitOfWork` per `create()` call
- [x] Update `src/v3/infra/postgres/CMakeLists.txt`
      — Changed from INTERFACE stub to STATIC library (`pvpgn_infra_postgresql`)
      — `find_package(PostgreSQL QUIET)` (CMake built-in finder)
      — `PVPGN_V3_WITH_POSTGRESQL` compile definition when library found
      — Links `pvpgn_core`, `pvpgn_application`, `pvpgn_infra_inmemory`
- [x] Add `add_subdirectory(infra/postgres)` to `src/v3/CMakeLists.txt`

## Files Changed
| File | Change |
|------|--------|
| `src/v3/infra/postgres/src/connection.cpp` | Created |
| `src/v3/infra/postgres/src/account_repository.cpp` | Created |
| `src/v3/infra/postgres/src/unit_of_work.cpp` | Created |
| `src/v3/infra/postgres/src/unit_of_work_factory.cpp` | Created |
| `src/v3/infra/postgres/include/infra/postgres/unit_of_work.hpp` | Modified |
| `src/v3/infra/postgres/CMakeLists.txt` | Modified |
| `src/v3/CMakeLists.txt` | Modified (add_subdirectory) |

## Notes
- Connection string format: `"host:port:user:password:database"`
  e.g. `"127.0.0.1:5432:pvpgn:secret:pvpgn_db"`
- SQL upsert: `INSERT INTO accounts … ON CONFLICT (id) DO UPDATE SET …`
- `void*` opaque `PGconn*` handle in header — no libpq headers in public API
- `Row` stores `PGresult*` + `row_num_` (int) — no per-row allocation
- Non-account repos use in-memory fallbacks (session-scoped data)
- `find_package(PostgreSQL)` is a CMake built-in (no extra module needed)
