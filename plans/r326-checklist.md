# R326 Checklist — Tests for FileAccountRepository

## Goal
Catch2 unit tests for `FileAccountRepository` using a temporary directory on
the real filesystem. Tests exercise the full CRUD surface of the repository
through the `IAccountRepository` port interface.

## Files created

- [x] `tests/unit/infra/file/file_account_repository_test.cpp`
- [x] `tests/unit/infra/file/CMakeLists.txt`

## Files modified

- [x] `tests/unit/infra/CMakeLists.txt` — added `add_subdirectory(file)` guard

## Test cases

| Test | Description |
|------|-------------|
| `save_creates_plain_file` | `save()` creates a `<Name>.plain` file on disk |
| `load_round_trip` | Save via one repo instance, load via a fresh instance |
| `find_by_name_returns_account` | `find_by_name()` returns the saved account |
| `find_nonexistent_returns_error` | `find_by_name()` returns error for unknown name |
| `remove_deletes_file` | `remove()` deletes the `.plain` file and makes find return error |
| `size_reflects_saved_accounts` | `size()` increments on save |
| `find_by_id_returns_account` | `find_by_id()` returns the saved account |

## Design notes

- `TempDir` RAII helper creates a unique temp directory under
  `std::filesystem::temp_directory_path()` and removes it on destruction.
- `make_test_account(id, username)` builds a minimal `Account` aggregate via
  `Account::rehydrate()` with a zero `BNHash` and default `Locale`.
- Tests are wrapped in `namespace pvpgn::infra::file` for ADL consistency.
- The `file_account_repository_test.cpp` uses the updated `IAccountRepository`
  interface (`AccountId`/`UserName` domain types) — no primitive types.

## Status: DONE
