# R327 Checklist — Tests for MigrationRunner

## Goal
Catch2 unit tests for `MigrationRunner` using injected fake `SqlExecutor` and
`VersionQuery` stubs. No real database is required — the runner is tested in
full isolation.

## Files created

- [x] `tests/unit/infra/migrations/migration_runner_test.cpp`
- [x] `tests/unit/infra/migrations/CMakeLists.txt`

## Files modified

- [x] `tests/unit/infra/CMakeLists.txt` — added `add_subdirectory(migrations)` guard

## Test cases

| Test | Description |
|------|-------------|
| `ensure_migration_table executes CREATE TABLE` | Verifies `_schema_migrations` DDL is executed |
| `ensure_migration_table propagates executor error` | Executor failure is surfaced as `Internal` |
| `current_version returns nullopt when no migrations applied` | `VersionQuery` returning `nullopt` |
| `current_version returns value from version_query` | `VersionQuery` returning a fixed version |
| `migrate_to_latest with empty span returns 0` | No SQL executed, returns version 0 |
| `migrate_to_latest applies all migrations on fresh db` | Both migration SQLs executed |
| `migrate_to_latest skips already-applied migrations` | No migration SQL when already at latest |
| `migrate_to applies only up to target version` | Only migration 1 SQL executed when target=1 |
| `migrate_to rejects unsorted migrations` | Returns `InvalidArgument` for reversed order |
| `migrate_to propagates executor failure` | Returns `Internal` when executor fails |

## Design notes

- `FakeExecutor` records all SQL strings and can be configured to fail on a
  substring match via `fail_on`.
- `FakeVersionQuery` returns a fixed `optional<uint32_t>`.
- Tests are wrapped in `namespace pvpgn::infra::migrations`.
- The `MigrationRunner` constructor takes `SqlExecutor` and `VersionQuery`
  by value (type-erased `std::function`), making injection trivial.

## Status: DONE
