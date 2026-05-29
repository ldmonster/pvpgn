# R322 — `pvpgn-migrate --from-plain --to-sqlite`

## Checklist
- [x] Read `src/v3/infra/file/include/infra/file/account_repository.hpp`
- [x] Read `src/v3/infra/file/src/account_repository.cpp` (`load_account_file()` implementation)
- [x] Read `src/v3/infra/sqlite/include/infra/sqlite/account_repository.hpp`
- [x] Read `src/v3/infra/sqlite/src/unit_of_work_factory.cpp` (how to open a SQLite DB)
- [x] Read `src/v3/infra/file/include/infra/file/flat_db_reader.hpp` (`parse_account_file`, `get_field`, `get_numeric_field`)
- [x] Implemented `migrate_plain_to_sqlite()` in `src/v3/app/pvpgn-migrate/main.cpp`:
  - Validates source directory exists
  - Opens/creates SQLite database via `SQLiteUnitOfWorkFactory` (runs schema migrations automatically)
  - Enumerates all `*.plain` files in source directory
  - For each file: reads content, parses key=value map via `parse_account_file()`
  - Extracts username, userid, passhash1, locale, auth_command_groups, auth_lock
  - Rehydrates `domain::identity::Account` via `Account::rehydrate()`
  - Saves each account via `IUnitOfWork::accounts().save()`
  - Wraps all saves in a single transaction (begin/commit/rollback)
  - Prints `"Migrated: <username>"` for each account
  - Prints `"Migration complete: N accounts migrated"` summary
  - Skips files that fail to parse, logs warning, continues
  - Returns exit code 0 on success, 1 on fatal error

## Result
Implemented the `--from-plain --to-sqlite` migration path in
`src/v3/app/pvpgn-migrate/main.cpp`. The `migrate_plain_to_sqlite()` function
uses `SQLiteUnitOfWorkFactory` to open/create the target database and
automatically apply all schema migrations. It then iterates over all `*.plain`
files in the source directory, parses each one using the existing
`parse_account_file()` / `get_field()` / `get_numeric_field()` helpers from
`pvpgn_infra_file`, rehydrates a `domain::identity::Account`, and saves it via
the `IUnitOfWork::accounts()` repository. All saves are wrapped in a single
transaction for efficiency. Errors on individual files are logged and skipped
without aborting the migration.
