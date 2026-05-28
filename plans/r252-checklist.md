# R252 — Wire TOML config into composition roots (logger init)

## Goal
Wire `infra::config::LogConfig` → `infra::log::make_and_install_logger()` into
every composition root so that `core::ILogger` is initialised from the TOML
config file at startup instead of using the default `NullLogger`.

## Checklist

### Infrastructure

- [x] **`src/v3/infra/log/include/infra/log/logger_factory.hpp`** — new header
  - Declares `make_and_install_logger(LogConfig, name) noexcept`
  - Bridges `infra::config::LogConfig` → `SpdlogConfig` → `core::set_default_logger()`

- [x] **`src/v3/infra/log/src/logger_factory.cpp`** — new implementation
  - Maps `LogConfig` fields to `SpdlogConfig`
  - Falls back to `StreamLogger(stderr, Info)` if spdlog init fails
  - Calls `core::set_default_logger()`

- [x] **`src/v3/CMakeLists.txt`** — `infra_log` target updated
  - Added `infra/log/src/logger_factory.cpp` to SOURCES
  - Added `$<$<TARGET_EXISTS:infra_config>:infra_config>` to PUBLIC_DEPS

### Config schema extensions

- [x] **`src/v3/infra/config/include/infra/config/d2cs_server_config.hpp`**
  - Extended `D2csLogSection` with: `file`, `stdout_sink`, `rotate_size`, `rotate_files`

- [x] **`src/v3/infra/config/src/d2cs_server_config.cpp`**
  - Updated `parse_log()` to read `stdout`, `rotate_size`, `rotate_files`, `file`

### Composition roots

- [x] **`src/v3/app/bnetd/src/main.cpp`**
  - Added `#include "core/format.hpp"`
  - Added conditional includes for `infra/config/server_config.hpp` and
    `infra/log/logger_factory.hpp` guarded by `PVPGN_V3_BNETD_HAVE_CONFIG`
  - Added logger init block after `build_config()`:
    loads `infra::config::ServerConfig` from `cli.config_path`, calls
    `infra::log::make_and_install_logger(infra_cfg.log, "bnetd")`
  - Replaced all startup/shutdown `std::cout` with `LOG_INFO`

- [x] **`src/v3/app/d2cs/src/main.cpp`**
  - Added `#include "core/format.hpp"`
  - Added conditional includes for `infra/config/d2cs_server_config.hpp`,
    `infra/config/server_config.hpp`, and `infra/log/logger_factory.hpp`
    guarded by `PVPGN_V3_D2CS_HAVE_CONFIG`
  - Added logger init block after `build_config()`:
    loads `infra::config::D2csServerConfig`, builds `LogConfig` from
    `D2csLogSection` fields (inline level parsing since `levels_str_to_level()`
    is not exported), calls `infra::log::make_and_install_logger(log_cfg, "d2cs")`
  - Replaced all startup/shutdown `std::cout` with `LOG_INFO`

- [x] **`src/v3/services/combined/src/main_combined.cpp`** — no change needed
  - The combined binary uses `pvpgn_core` / `pvpgn_runtime` targets from a
    separate build system (`PVPGN_SINGLE_BINARY=ON`) that does not include the
    v3 `infra_log` / `infra_config` targets. Logger wiring is deferred until
    the combined binary is migrated to the v3 build system.

### CMake target linkage

- [x] **`src/v3/app/bnetd/CMakeLists.txt`**
  - Added optional `infra_log` linkage block

- [x] **`src/v3/app/d2cs/CMakeLists.txt`**
  - Added optional `infra_config` linkage block with `PVPGN_V3_D2CS_HAVE_CONFIG` define
  - Added optional `infra_log` linkage block

### Config templates

- [x] **`conf/bnetd.toml.in`** — `[log]` section extended with:
  - `#file = "${LOCALSTATEDIR}/bnetd.log"` (commented out — opt-in)
  - `stdout = true`
  - `rotate_size = 10485760`
  - `rotate_files = 5`

- [x] **`conf/d2cs.toml.in`** — `[log]` section extended with:
  - `#file = "${LOCALSTATEDIR}/d2cs.log"` (commented out — opt-in)
  - `stdout = true`
  - `rotate_size = 10485760`
  - `rotate_files = 5`

## Key design decisions

1. **`levels_str_to_level()` not exported** — The function is `static` in
   `server_config.cpp`. The d2cs composition root uses inline substring matching
   (`lvls.find("trace")`, etc.) to parse the levels string.

2. **`D2csLogSection` extended** — Rather than only having `levels`, the struct
   now carries `file`, `stdout_sink`, `rotate_size`, `rotate_files` so the TOML
   keys are actually consumed by the parser.

3. **Fallback logger** — If spdlog init fails (e.g. bad file path), a
   `StreamLogger(stderr, Info)` is installed so the process never runs with a
   `NullLogger`.

4. **`#if defined(PVPGN_V3_BNETD_HAVE_LOGGER_FACTORY)`** guards — The logger
   init block is compiled only when both `infra_config` and `infra_log` targets
   are available (i.e. `PVPGN_V3_WITH_TOMLPP=ON` and `PVPGN_V3_WITH_SPDLOG=ON`).
   IntelliSense may show false-positive errors in the `#if` block because the
   define is only set by CMake at build time.

## Files changed

| File | Change |
|------|--------|
| `src/v3/infra/log/include/infra/log/logger_factory.hpp` | **NEW** |
| `src/v3/infra/log/src/logger_factory.cpp` | **NEW** |
| `src/v3/CMakeLists.txt` | Modified — `infra_log` sources + deps |
| `src/v3/infra/config/include/infra/config/d2cs_server_config.hpp` | Modified — `D2csLogSection` extended |
| `src/v3/infra/config/src/d2cs_server_config.cpp` | Modified — `parse_log()` extended |
| `src/v3/app/bnetd/CMakeLists.txt` | Modified — optional `infra_log` linkage |
| `src/v3/app/bnetd/src/main.cpp` | Modified — logger init + LOG_INFO |
| `src/v3/app/d2cs/CMakeLists.txt` | Modified — optional `infra_config`/`infra_log` linkage |
| `src/v3/app/d2cs/src/main.cpp` | Modified — logger init + LOG_INFO |
| `conf/bnetd.toml.in` | Modified — new `[log]` keys |
| `conf/d2cs.toml.in` | Modified — new `[log]` keys |
