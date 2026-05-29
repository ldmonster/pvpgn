# R323 — `pvpgn-migrate --from-plain --to-toml-file`

## Checklist
- [x] Read `src/v3/infra/file/include/infra/file/account_repository.hpp`
- [x] Read `src/v3/infra/file/src/account_repository.cpp`
- [x] Searched for existing TOML serialization in `src/v3/infra/file/` (none found — hand-rolled)
- [x] Reviewed `src/v3/tools/conf_converter/main.cpp` for TOML escaping patterns
- [x] Implemented `migrate_plain_to_toml()` in `src/v3/app/pvpgn-migrate/main.cpp`:
  - Validates source directory exists
  - Creates destination directory (including parents) if it does not exist
  - Enumerates all `*.plain` files in source directory
  - For each file: reads content, parses key=value map via `parse_account_file()`
  - Extracts: username, passhash1, email, userid, flags (auth_command_groups),
    created (created_at), lastlogin_time (last_login), lastlogin_ip (last_login_ip)
  - Serializes to TOML format:
    ```toml
    [account]
    username = "..."
    passhash1 = "..."
    email = "..."
    userid = 0
    flags = 0

    [timestamps]
    created_at = 0
    last_login = 0

    [network]
    last_login_ip = "..."
    ```
  - Writes output to `<dst_dir>/<username>.toml`
  - Implements `toml_escape()` helper for safe double-quoted TOML strings
  - Prints `"Converted: <username>"` for each account
  - Prints `"Conversion complete: N accounts converted"` summary
  - Skips files that fail to parse, logs warning, continues
  - Returns exit code 0 on success, 1 on fatal error

## Result
Implemented the `--from-plain --to-toml-file` conversion path in
`src/v3/app/pvpgn-migrate/main.cpp`. The `migrate_plain_to_toml()` function
iterates over all `*.plain` files in the source directory, parses each one
using the existing `parse_account_file()` / `get_field()` / `get_numeric_field()`
helpers from `pvpgn_infra_file`, and writes a structured TOML file per account
to the destination directory. The TOML output has three sections: `[account]`
(identity fields), `[timestamps]` (created_at, last_login), and `[network]`
(last_login_ip). String values are escaped via a `toml_escape()` helper that
handles all TOML special characters. No external TOML library is required for
writing. Errors on individual files are logged and skipped without aborting.
