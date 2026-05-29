# R314 — Implement `FileAccountRepository::load_account_file()`

## Checklist
- [x] Read `src/v3/infra/file/include/infra/file/account_repository.hpp` to understand the interface
- [x] Read `src/v3/infra/file/src/account_repository.cpp` to understand the existing stub
- [x] Read `src/v3/infra/file/include/infra/file/flat_db_reader.hpp` — existing `.plain` parser utility
- [x] Read `src/v3/infra/file/src/flat_db_reader.cpp` — `parse_account_file()` / `get_field()` / `get_numeric_field()`
- [x] Read `src/v3/domain/identity/account.hpp` — `Account::rehydrate()` signature
- [x] Open file via `std::ifstream` at `data_dir_ / filename`
- [x] Return `std::nullopt` if file cannot be opened
- [x] Parse content with `parse_account_file()` (key=value, `#` comments, backslash-separated hierarchy)
- [x] Extract `BNET\acct\username` — return `std::nullopt` if missing
- [x] Extract `BNET\acct\passhash1` — decode 40-hex string to `domain::BNHash` via `bn_hash_from_hex()`
- [x] Extract `BNET\acct\userid` — derive `domain::AccountId`
- [x] Extract `BNET\acct\locale` — parse via `domain::Locale::parse_or_default()`
- [x] Extract `BNET\acct\auth_command_groups` — decode bitmask into `CommandGroupMask`
- [x] Extract `BNET\acct\auth_lock` — decode to `bool locked`
- [x] Construct and return `domain::identity::Account` via `Account::rehydrate()`
- [x] Use `std::string_view` for key matching (via `get_field()` helper)

## Result
`FileAccountRepository::load_account_file()` is fully implemented in
`src/v3/infra/file/src/account_repository.cpp` (lines 100–171).
It opens the `.plain` file with `std::ifstream`, reads the entire content,
delegates parsing to the existing `parse_account_file()` / `get_field()` /
`get_numeric_field()` utilities from `flat_db_reader`, maps all legacy
`BNET\acct\*` keys to domain fields, and constructs the `Account` via
`Account::rehydrate()`. Returns `std::nullopt` if the file cannot be opened
or if the required `BNET\acct\username` key is absent.
