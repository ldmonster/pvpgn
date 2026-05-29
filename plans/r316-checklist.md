# R316 — Reconcile dual SQLite implementations

## Checklist
- [x] Read canonical implementation: `src/v3/infra/sqlite/src/account_repository.cpp`
- [x] Read canonical header: `src/v3/infra/sqlite/include/infra/sqlite/account_repository.hpp`
- [x] Read deprecated implementation: `src/v3/infra/persistence/sqlite/src/sqlite_account_repository.cpp`
- [x] Read deprecated header: `src/v3/infra/persistence/sqlite/include/infra/persistence/sqlite/sqlite_account_repository.hpp`
- [x] Read `src/v3/infra/persistence/sqlite/CMakeLists.txt` — confirm it is disabled
- [x] Read `src/v3/infra/persistence/CMakeLists.txt` — confirm it does NOT link deprecated impl
- [x] Read `src/v3/infra/persistence/src/backend_registration.cpp` — confirm it uses canonical `infra/sqlite`
- [x] Disable `infra/persistence/sqlite` build target in its `CMakeLists.txt` (replaced with `message(STATUS ...)` only — no `add_library`)
- [x] Add `#error` guard to deprecated header to prevent accidental inclusion
- [x] Ensure `infra/persistence/CMakeLists.txt` does NOT add `infra/persistence/sqlite` as a subdirectory
- [x] Ensure `backend_registration.cpp` includes `infra/sqlite/unit_of_work_factory.hpp` (canonical)
- [x] Ensure `backend_registration.cpp` does NOT reference `infra/persistence/sqlite`
- [x] Verify only ONE SQLite account repository is compiled and linked (`pvpgn_infra_sqlite`)

## Result
The dual SQLite implementation conflict is fully resolved:

1. **Canonical implementation** (`src/v3/infra/sqlite/`) — `SQLiteAccountRepository` in namespace
   `pvpgn::infra::sqlite` — is the only compiled and linked implementation.

2. **Deprecated implementation** (`src/v3/infra/persistence/sqlite/`) — `SqliteAccountRepository`
   in namespace `pvpgn::infra::persistence::sqlite` — is completely disabled:
   - `CMakeLists.txt` contains only a `message(STATUS "... DEPRECATED — skipped ...")` with no
     `add_library` or `target_*` calls, so it produces no build artifact.
   - The header has an `#error` preprocessor guard that prevents accidental inclusion.
   - The source files remain on disk for historical reference but are never compiled.

3. **`backend_registration.cpp`** includes `infra/sqlite/unit_of_work_factory.hpp` and
   `infra/file/unit_of_work_factory.hpp` exclusively — no reference to the deprecated path.

4. **`infra/persistence/CMakeLists.txt`** does NOT add `persistence/sqlite` as a subdirectory,
   so there is no ODR violation risk.
