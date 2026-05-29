# R331 Checklist — `core::Secret<T>` + env-var override layer

## Goal
Prevent accidental logging of sensitive config values (passwords, DSNs) and
allow runtime overrides via environment variables without editing the TOML file.

## Steps

- [x] **Create `src/v3/core/include/core/secret.hpp`**
  - `Secret<T>` template: non-copyable, movable, `operator<<` prints `"***"`,
    `reveal()` returns the underlying value.
  - `Secret<std::string>` specialisation: zeroing destructor, `from_string()`
    factory supporting `env:<VAR>`, `file:<path>`, and literal resolution.

- [x] **Update `src/v3/infra/config/include/infra/config/server_config.hpp`**
  - Added `#include "core/secret.hpp"`.
  - `PersistenceConfig::dsn` → `core::Secret<std::string>`
  - `StorageConfig::dsn` → `core::Secret<std::string>`
  - `WolConfig::wol_autoupdate_password` → `core::Secret<std::string>`

- [x] **Update `src/v3/infra/config/src/server_config.cpp`**
  - `parse_persistence()`: uses `Secret<std::string>::from_string()` for `dsn`.
  - `parse_storage()`: uses `Secret<std::string>::from_string()` for `dsn`.
  - `parse_wol()`: uses `Secret<std::string>::from_string()` for `wol_autoupdate_password`.
  - Added `apply_env_overrides(ServerConfig&)` scanning `environ` for
    `PVPGN_BNETD__<SECTION>__<KEY>` variables.
  - `load_server_config()` calls `apply_env_overrides()` after TOML parsing.
  - `parse_server_config()` does **not** apply env overrides (used in tests).

- [x] **Fix downstream consumers**
  - `src/v3/infra/config/include/infra/config/legacy_prefs.hpp`:
    - Constructor init: `wol_autoupdate_password_str_` uses `.reveal()`.
    - `storage_dsn()` accessor uses `.reveal()`.
  - `src/v3/app/bnetd/src/main.cpp`:
    - `persistence_dsn = result.value().persistence.dsn.reveal()`.

- [x] **Create `tests/unit/core/secret_test.cpp`**
  - Tests: `operator<<` prints `***`, `reveal()` returns value,
    `from_string` with literal / `env:` / `file:` / unset-var / missing-file,
    move semantics.

- [x] **Update `tests/unit/core/CMakeLists.txt`**
  - Added `pvpgn_v3_add_test(test_core_secret SOURCES secret_test.cpp DEPS core)`.
  - Added `core/secret.hpp` to `pvpgn_v3_add_header_selfcheck` HEADERS list.

## Notes
- `legacy_prefs.hpp` is actively used by `prefs_bridge.cpp`,
  `prefs_dump.hpp`, and `d2dbs_prefs_bridge.cpp` — it was updated to use
  `.reveal()` but **not** deleted.
- The env-var override table uses a static array of `(section, key, setter)`
  triples to avoid a large if/else chain.
