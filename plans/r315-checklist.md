# R315 — Atomic file write-back in `FileAccountRepository::save()`

## Checklist
- [x] Update in-memory cache first under `std::unique_lock<std::shared_mutex>`
- [x] Return early on cache save failure
- [x] Serialize account to `.plain` key=value format via `std::ostringstream`
- [x] Write `BNET\acct\username` field
- [x] Write `BNET\acct\passhash1` field (40-hex via `bn_hash_to_hex()`)
- [x] Write `BNET\acct\auth_lock` field (0 or 1)
- [x] Write `BNET\acct\auth_command_groups` field (bitmask)
- [x] Write `BNET\acct\locale` field
- [x] Write `BNET\acct\userid` field
- [x] Determine final path: `data_dir_ / username.plain`
- [x] Determine temp path: `data_dir_ / username.plain.tmp`
- [x] Open temp file with `::open()` using `O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC` and mode `0600`
- [x] Return error status if `::open()` fails
- [x] Write all content bytes in a loop using `::write()`
- [x] Return error status if `::write()` fails (close fd first)
- [x] Call `::fsync(fd)` before closing — return error if it fails
- [x] Close the file descriptor
- [x] Atomically rename `.tmp` → `.plain` via `std::filesystem::rename()`
- [x] Return error status if rename fails
- [x] Return `core::ok()` on success
- [x] Serialization format is compatible with `load_account_file()` parser (R314)

## Result
`FileAccountRepository::save()` is fully implemented in
`src/v3/infra/file/src/account_repository.cpp` (lines 177–258).
It first updates the in-memory cache under a write lock, then serializes the
account to the legacy `.plain` key=value format, writes to a `.plain.tmp`
temp file using low-level POSIX I/O (`::open` / `::write` / `::fsync`),
and atomically renames the temp file to the final `.plain` path via
`std::filesystem::rename`. On any I/O error the function returns a
`core::Error{StatusCode::Internal, ...}` status. The serialized format is
fully round-trip compatible with the `load_account_file()` parser.
