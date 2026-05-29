# R325 Checklist — Tests for SQLiteAccountRepository

## Goal
Catch2 unit tests for `SQLiteAccountRepository` using an in-memory SQLite
database (`:memory:`). Tests exercise the full CRUD surface of the repository
through the `IAccountRepository` port interface.

## Files created

- [x] `tests/unit/infra/sqlite/sqlite_account_repository_test.cpp`
- [x] `tests/unit/infra/sqlite/CMakeLists.txt`

## Files modified

- [x] `tests/unit/infra/CMakeLists.txt` — added `add_subdirectory(sqlite)` guard

## Test cases

| Test | Description |
|------|-------------|
| `save_and_find_by_id` | Save an account, retrieve it by `AccountId` |
| `save_and_find_by_name` | Save an account, retrieve it by `UserName` |
| `find_by_id_not_found` | Returns `NotFound` error for unknown ID |
| `find_by_name_not_found` | Returns `NotFound` error for unknown name |
| `save_updates_existing` | Second `save()` with same ID overwrites the record |
| `remove_deletes_account` | `remove()` makes subsequent `find_by_id` return `NotFound` |
| `size_reflects_saved_accounts` | `size()` increments on save, decrements on remove |
| `forEach_iterates_all` | `forEach` visits every saved account |

## Design notes

- `make_in_memory_db()` helper creates a `SQLiteConnection` with `":memory:"`
  and runs `MigrationRunner` to apply all schema migrations before each test.
- `make_test_account(id, username)` builds a minimal `Account` aggregate via
  `Account::rehydrate()` with a zero `BNHash` and default `Locale`.
- Tests are wrapped in `namespace pvpgn::infra::sqlite` for ADL consistency.

## Status: DONE
