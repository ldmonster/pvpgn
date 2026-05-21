# Phase 1 Step 10 — TOML Configuration Migration Checklist

## Status: 🔄 In Progress (R121)

## Objective

Introduce a stable, exception-free C++20 wrapper (`Config`) over the
`tomlplusplus` library already present in `infra_config`, create a TOML
template for `bnetd.conf`, and write Catch2 unit tests.  The legacy
`prefs.cpp`/`prefs.h` parser is **not** modified in this step.

---

## Deliverables

| # | File | Status |
|---|------|--------|
| 1 | `src/v3/infra/config/include/infra/config/config.hpp` | ✅ Created R120 |
| 2 | `conf/bnetd.toml.in` | ✅ Created R120 |
| 3 | `tests/unit/infra/config/test_config.cpp` | ✅ Created R120 |
| 4 | `tests/unit/infra/config/CMakeLists.txt` — add `test_infra_config_config` | ✅ Updated R120 |
| 5 | `plans/step10-toml-checklist.md` (this file) | ✅ Created R120 |
| 6 | `plans/progress-master.md` — Step 10 in-progress + R120 session log | ✅ Updated R120 |
| 7 | `src/v3/infra/config/include/infra/config/server_config.hpp` — expanded to all 20 TOML sections | ✅ Updated R121 |
| 8 | `src/v3/infra/config/src/server_config.cpp` — rewritten to use `Config` wrapper | ✅ Updated R121 |
| 9 | `src/v3/infra/config/include/infra/config/legacy_prefs.hpp` — expanded to all ~100 `prefs_get_*` accessors | ✅ Updated R121 |
| 10 | `tests/unit/infra/config/server_config_test.cpp` — updated for new struct layout | ✅ Updated R121 |
| 11 | `tests/unit/infra/config/legacy_prefs_test.cpp` — updated + new policy/timing/clan test case | ✅ Updated R121 |
| 12 | `conf/d2cs.toml.in` — TOML template for d2cs (5 sections) | ✅ Created R121 |
| 13 | `conf/d2dbs.toml.in` — TOML template for d2dbs (5 sections) | ✅ Created R121 |

---

## Audit: Legacy Config Consumer Inventory (`src/bnetd/`)

The legacy config API is exposed via `src/bnetd/prefs.h` / `prefs.cpp`.
All callers use `prefs_get_*()` / `prefs_set_*()` free functions.

### Files that `#include "prefs.h"` (grep result, R120)

| File | prefs_get_* call count (approx) | Migration priority |
|------|--------------------------------|-------------------|
| `src/bnetd/main.cpp` | ~15 | High — startup/init |
| `src/bnetd/server.cpp` | ~20 | High — main loop |
| `src/bnetd/connection.cpp` | ~25 | High — per-connection |
| `src/bnetd/handle_bnet.cpp` | ~30 | High — protocol handler |
| `src/bnetd/command.cpp` | ~20 | Medium |
| `src/bnetd/message.cpp` | ~8 | Medium |
| `src/bnetd/game.cpp` | ~10 | Medium |
| `src/bnetd/channel.cpp` | ~5 | Medium |
| `src/bnetd/account.cpp` | ~5 | Medium |
| `src/bnetd/irc.cpp` | ~5 | Medium |
| `src/bnetd/handle_irc.cpp` | ~5 | Medium |
| `src/bnetd/handle_irc_common.cpp` | ~6 | Medium |
| `src/bnetd/handle_wol.cpp` | ~3 | Low |
| `src/bnetd/handle_wserv.cpp` | ~5 | Low |
| `src/bnetd/handle_apireg.cpp` | ~1 | Low |
| `src/bnetd/handle_d2cs.cpp` | ~1 | Low |
| `src/bnetd/handle_init.cpp` | ~2 | Low |
| `src/bnetd/tracker.cpp` | ~5 | Low |
| `src/bnetd/ladder.cpp` | ~3 | Low |
| `src/bnetd/output.cpp` | ~3 | Low |
| `src/bnetd/userlog.cpp` | ~4 | Low |
| `src/bnetd/mail.cpp` | ~3 | Low |
| `src/bnetd/ipban.cpp` | ~4 | Low |
| `src/bnetd/support.cpp` | ~2 | Low |
| `src/bnetd/topic.cpp` | ~2 | Low |
| `src/bnetd/watch.cpp` | ~2 | Low |
| `src/bnetd/clan.cpp` | ~2 | Low |
| `src/bnetd/storage_file.cpp` | ~4 | Low |
| `src/bnetd/sql_common.cpp` | ~3 | Low |
| `src/bnetd/sql_dbcreator.cpp` | ~1 | Low |
| `src/bnetd/attrgroup.cpp` | ~4 | Low |
| `src/bnetd/attrlayer.cpp` | ~2 | Low |
| `src/bnetd/account_wrap.cpp` | ~4 | Low |
| `src/bnetd/versioncheck.cpp` | ~1 | Low |
| `src/bnetd/anongame.cpp` | ~1 | Low |
| `src/bnetd/anongame_maplists.cpp` | ~2 | Low |
| `src/bnetd/luainterface.cpp` | ~80 | Low (Lua bridge) |
| `src/bnetd/i18n.cpp` | ~3 | Low |
| `src/bnetd/icons.cpp` | ~1 | Low |

**Total**: ~38 files, ~300+ call sites.

### prefs_get_* accessor inventory

All keys from `conf/bnetd.conf.in` are covered by `prefs_get_*` accessors.
The TOML sections in `conf/bnetd.toml.in` map as follows:

| TOML section | Legacy prefs_get_* prefix | Key count |
|---|---|---|
| `[privileges]` | `prefs_get_effective_user/group` | 2 |
| `[storage]` | `prefs_get_storage_path` | 1 |
| `[files]` | `prefs_get_filedir`, `prefs_get_scriptdir`, … | ~25 |
| `[localization]` | `prefs_get_localizefile`, `prefs_get_motdfile`, … | 7 |
| `[log]` | `prefs_get_loglevels`, `prefs_get_logfile` | 2 |
| `[d2cs]` | `prefs_get_d2cs_version`, `prefs_allow_d2cs_setname` | 2 |
| `[downloads]` | `prefs_get_iconfile`, `prefs_get_war3_iconfile`, `prefs_get_star_iconfile` | 3 |
| `[client_verification]` | `prefs_get_allowed_clients`, `prefs_get_allow_bad_version`, `prefs_get_allow_unknown_version` | 3 |
| `[timing]` | `prefs_get_user_sync_timer`, `prefs_get_user_flush_timer`, … | 8 |
| `[policy]` | `prefs_get_allow_new_accounts`, `prefs_get_max_accounts`, … | ~20 |
| `[account]` | `prefs_get_savebyname`, `prefs_get_sync_on_logoff`, … | 6 |
| `[tracking]` | `prefs_get_track`, `prefs_get_trackserv_addrs`, … | 6 |
| `[network]` | `prefs_get_servername`, `prefs_get_max_connections`, … | 12 |
| `[wol]` | `prefs_get_wolv1_addrs`, `prefs_get_wol_timezone`, … | 9 |
| `[irc]` | `prefs_get_irc_addrs`, `prefs_get_irc_network_name`, … | 4 |
| `[telnet]` | `prefs_get_telnet_addrs` | 1 |
| `[ladder]` | `prefs_get_war3_ladder_update_secs`, `prefs_get_XML_output_ladder` | 2 |
| `[status]` | `prefs_get_output_update_secs`, `prefs_get_XML_status_output` | 2 |
| `[clan]` | `prefs_get_clan_newer_time`, `prefs_get_clan_max_members`, … | 4 |
| `[command_log]` | `prefs_get_log_commands`, `prefs_get_log_command_groups`, `prefs_get_log_command_list` | 3 |

---

## CMake Notes

- **Option name**: `PVPGN_V3_WITH_TOMLPP` (NOT `PVPGN_V3_WITH_TOMLPLUSPLUS`)
- **CMake target**: `tomlplusplus::tomlplusplus`
- **`infra_config` library**: already defined in `src/v3/CMakeLists.txt` lines 256–272
  with `PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/infra/config/include`
- **`config.hpp`** is header-only; no new `.cpp` source or CMakeLists.txt entry needed
- **`tests/unit/infra/CMakeLists.txt`**: already has `add_subdirectory(config)` guarded
  by `if(PVPGN_V3_WITH_TOMLPP)` — no change needed

---

## infra/config files (post-R121)

| File | Purpose | Status |
|------|---------|--------|
| `include/infra/config/config.hpp` | Header-only `Config` wrapper over `toml::table` | ✅ R120 |
| `include/infra/config/server_config.hpp` | Typed `ServerConfig` struct (20 sections) + factory functions | ✅ Expanded R121 |
| `include/infra/config/legacy_prefs.hpp` | `LegacyPrefs` adapter — all ~100 `prefs_get_*` accessors | ✅ Expanded R121 |
| `include/infra/config/config_watcher.hpp` | `ConfigWatcher` — file-based hot-reload with subscriber notification | Pre-existing |
| `src/server_config.cpp` | Uses `Config` wrapper; parses all 20 TOML sections | ✅ Rewritten R121 |
| `src/config_watcher.cpp` | Implementation of `ConfigWatcher` | Pre-existing |

---

## New file: `config.hpp` API summary

```cpp
namespace pvpgn::infra::config {

class Config {
public:
    // Construction
    static std::optional<Config> load_string(std::string_view toml_text) noexcept;
    static std::optional<Config> load_file(std::string_view path) noexcept;

    // Value access
    template <typename T>
    std::optional<T> get(std::string_view key) const noexcept;

    template <typename T>
    T get_or(std::string_view key, T fallback) const noexcept;

    bool has(std::string_view key) const noexcept;
    std::vector<std::string> keys() const;

    // Sub-table access
    std::optional<Config> section(std::string_view section_name) const noexcept;

    // Raw access
    const toml::table& raw() const noexcept;
};

}  // namespace pvpgn::infra::config
```

---

## Test coverage (`test_config.cpp`)

| Test case | What it verifies |
|-----------|-----------------|
| `load_string: empty input` | Empty TOML → valid Config with no keys |
| `load_string: syntax error` | Bad TOML → `nullopt` |
| `load_string: valid TOML` | Well-formed TOML → `has_value()` |
| `load_file: missing file` | Non-existent path → `nullopt` |
| `load_file: reads from disk` | Temp file → `has_value()` |
| `get: returns value for existing key` | `string`, `int64_t`, `bool` all work |
| `get: returns nullopt for missing key` | Absent key → `nullopt` |
| `get_or: returns value when key exists` | Existing key overrides fallback |
| `get_or: returns fallback when key absent` | Missing key → fallback |
| `has: true/false` | Presence check |
| `keys: returns all top-level keys` | 3-key table → 3 entries |
| `keys: empty table` | Empty → empty vector |
| `section: returns child Config` | `[network]` sub-table accessible |
| `section: absent section` | Missing section → `nullopt` |
| `section: key is not a table` | Scalar key → `nullopt` |
| `section: nested sections` | `[outer.inner]` → two-level `section()` chain |
| `raw: exposes toml::table` | Direct `toml::table` access works |
| `bnetd.toml representative round-trip` | All 6 bnetd sections parse correctly |

---

## Round 122 deliverables

- **`src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/prefs_bridge.hpp`** — NEW:
  C-linkage bridge header; `pvpgn_v3_prefs_load_toml(path)`, `pvpgn_v3_prefs_loaded()`, and ~100
  `pvpgn_v3_prefs_get_*` / `pvpgn_v3_prefs_allow_*` declarations
- **`src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp`** — NEW: bridge implementation;
  `std::optional<LegacyPrefs> g_prefs` global; uses `load_server_config(std::filesystem::path)`
- **`src/v3/CMakeLists.txt`** — MODIFIED: `prefs_bridge.cpp` added to `integration_legacy_bnetd`
  SOURCES; `$<$<TARGET_EXISTS:infra_config>:infra_config>` added to PUBLIC_DEPS
- **`src/v3/infra/config/include/infra/config/legacy_prefs.hpp`** — MODIFIED: added
  `effective_user()` and `effective_group()` public accessors (were stored as private strings)
- **`src/bnetd/prefs.cpp`** — MODIFIED: all ~100 `prefs_get_*` / `prefs_allow_*` functions now
  have `#ifdef PVPGN_V3_BNETD_INTEGRATION` delegation guards; when TOML is loaded, each function
  delegates to the corresponding `pvpgn_v3_prefs_get_*()` bridge function
- **`src/bnetd/main.cpp`** — MODIFIED: added `# include "integration/legacy_bnetd/prefs_bridge.hpp"`
  to the `PVPGN_V3_BNETD_INTEGRATION` block; after `prefs_load()` succeeds, derives TOML path by
  replacing `.conf` extension with `.toml` and calls `pvpgn_v3_prefs_load_toml()`; non-fatal if
  TOML file absent (legacy `.conf` values remain active as fallback)

## Deferred to future steps (Round 123+)

- Migrating `prefs_get_*` callers in `src/bnetd/` to use `LegacyPrefs` accessors directly
  - Priority order: 1-call files first (icons, versioncheck, anongame, sql_dbcreator, handle_apireg,
    handle_d2cs, handle_init, support, topic) → medium files → high-impact files last
  - Each migration: replace `#include "prefs.h"` with `LegacyPrefs` reference; replace
    `prefs_get_X()` with `legacy_prefs.X()`
- Deleting `src/bnetd/prefs.cpp` / `prefs.h` (after all 38 callers migrated)
- Installing `bnetd.toml.in`, `d2cs.toml.in`, `d2dbs.toml.in` via `conf/CMakeLists.txt`
- Wiring `ConfigWatcher` into the bnetd startup path (hot-reload support)
- Expanding `d2cs` and `d2dbs` to have their own typed config structs (analogous to `ServerConfig`)
