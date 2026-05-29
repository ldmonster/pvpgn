# R318 — Wire `MigrationRunner` into `SQLiteUnitOfWorkFactory` startup

## Checklist
- [x] Read `SQLiteUnitOfWorkFactory` header (`unit_of_work_factory.hpp`)
- [x] Read `SQLiteUnitOfWorkFactory` source (`unit_of_work_factory.cpp`)
- [x] Read `MigrationRunner` header and source
- [x] Verified `unit_of_work_factory.cpp` already includes `infra/migrations/migration_runner.hpp` and `infra/migrations/all_migrations.hpp`
- [x] Verified constructor opens `SQLiteConnection`, constructs `MigrationRunner` with `exec` and version-query lambdas, calls `ensure_migration_table()` and `migrate_to_latest(get_all_migrations())`
- [x] Verified `src/v3/infra/sqlite/CMakeLists.txt` already links `pvpgn_infra_migrations`
- [x] No changes required — implementation was already complete and correct

## Result
`SQLiteUnitOfWorkFactory` already had `MigrationRunner` fully wired in its constructor: it opens the shared `SQLiteConnection`, constructs a `MigrationRunner` with SQL-executor and version-query lambdas, calls `ensure_migration_table()`, and then `migrate_to_latest(get_all_migrations())`. The `CMakeLists.txt` already linked `pvpgn_infra_migrations`. No code changes were needed for R318.
