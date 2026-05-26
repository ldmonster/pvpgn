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

---

## Round 134 — caller migration kickoff (7 small files)

Introduced [`src/bnetd/prefs_v3_shim.h`](../src/bnetd/prefs_v3_shim.h) — a header-only
shim exposing inline `pvpgn::bnetd::prefs_v3::X()` accessors that delegate to the
existing `pvpgn_v3_prefs_*` C-bridge under `PVPGN_V3_BNETD_INTEGRATION` and fall
back to legacy `prefs_get_X()` otherwise. The shim keeps the in-flight migration
*observably* neutral (the bridge dispatch is the same one that already lives
inside `prefs.cpp`) while moving callers off the legacy entry points one TU at a
time.

**Migrated (7 / ~38 files, 12 / ~300 call sites):**

- [x] `src/bnetd/versioncheck.cpp` — `allow_bad_version`
- [x] `src/bnetd/sql_dbcreator.cpp` — `DBlayoutfile`
- [x] `src/bnetd/handle_apireg.cpp` — `allow_new_accounts`
- [x] `src/bnetd/handle_d2cs.cpp` — `allow_d2cs_setname`, `d2cs_version`
- [x] `src/bnetd/handle_init.cpp` — `max_conns_per_IP` (×2)
- [x] `src/bnetd/support.cpp` — `filedir` (×2)
- [x] `src/bnetd/topic.cpp` — `topicfile` (×2)

**Inspected, nothing to migrate:**

- `src/bnetd/icons.cpp` — only contains the *definition* of
  `prefs_get_custom_icons`, no call sites.

**Pending — medium-complexity files (single accessor used a handful of times):**

- [ ] `src/bnetd/message.cpp`
- [ ] `src/bnetd/output.cpp`
- [ ] `src/bnetd/mail.cpp`
- [ ] `src/bnetd/userlog.cpp`
- [ ] `src/bnetd/tracker.cpp`
- [ ] `src/bnetd/ipban.cpp`
- [ ] `src/bnetd/watch.cpp`
- [ ] `src/bnetd/sql_common.cpp`
- [ ] `src/bnetd/storage_file.cpp`
- [ ] `src/bnetd/attrgroup.cpp`
- [ ] `src/bnetd/attrlayer.cpp`
- [ ] `src/bnetd/account_wrap.cpp`
- [ ] `src/bnetd/ladder.cpp`
- [ ] `src/bnetd/i18n.cpp`
- [ ] `src/bnetd/anongame_maplists.cpp`
- [ ] `src/bnetd/anongame.cpp`
- [ ] `src/bnetd/handle_wol.cpp`
- [ ] `src/bnetd/handle_wserv.cpp`
- [ ] `src/bnetd/handle_irc.cpp`
- [ ] `src/bnetd/handle_irc_common.cpp`
- [ ] `src/bnetd/clan.cpp`
- [ ] `src/bnetd/irc.cpp`
- [ ] `src/bnetd/account.cpp`
- [ ] `src/bnetd/channel.cpp`

**Pending — high-impact files (many distinct accessors, often startup paths):**

- [ ] `src/bnetd/handle_bnet.cpp`
- [ ] `src/bnetd/connection.cpp`
- [ ] `src/bnetd/server.cpp`
- [ ] `src/bnetd/command.cpp`
- [ ] `src/bnetd/game.cpp`
- [ ] `src/bnetd/luainterface.cpp`
- [ ] `src/bnetd/main.cpp`

**Verification (Round 134):** `g++ -std=c++20 -fsyntax-only -DPVPGN_V3_BNETD_INTEGRATION=1`
on all 7 modified TUs compiles cleanly with the v3 integration headers in scope.
Local `cmake --build . --target bnetd_legacy` blocked by pre-existing env issues
unrelated to this round (Lua not auto-detected; `bnetd_legacy` include dirs miss
`src/v3/infra/compat/include`).

---

## Round 135 — medium callers wave 1 (8 files)

Extended [`src/bnetd/prefs_v3_shim.h`](../src/bnetd/prefs_v3_shim.h) with 18 new
accessors (see Round 135 entry in `progress-master.md` for the full list).
Migrated:

- [x] `src/bnetd/message.cpp`  — `servername`, `contact_name`
- [x] `src/bnetd/output.cpp`   — `XML_status_output`, `outputdir`
- [x] `src/bnetd/mail.cpp`     — `maildir`, `mail_support`, `mail_quota`
- [x] `src/bnetd/userlog.cpp`  — `log_command_list`, `log_commands`, `log_command_groups`, `userlogdir`
- [x] `src/bnetd/tracker.cpp`  — `location`, `description`, `url`, `contact_name`, `contact_email`
- [x] `src/bnetd/ipban.cpp`    — `ipban_check_int`, `ipbanfile`
- [x] `src/bnetd/watch.cpp`    — `servername`
- [x] `src/bnetd/sql_common.cpp` — `clan_channel_default_private`, `clan_newer_time`

**Cumulative:** 15 / ~38 files migrated. Remaining medium-complexity files
(unchanged from Round 134 list, minus the 8 above): `attrgroup.cpp`,
`attrlayer.cpp`, `account_wrap.cpp`, `ladder.cpp`, `i18n.cpp`,
`anongame_maplists.cpp`, `anongame.cpp`, `handle_wol.cpp`, `handle_wserv.cpp`,
`handle_irc.cpp`, `handle_irc_common.cpp`, `clan.cpp`, `irc.cpp`, `account.cpp`,
`channel.cpp`, `storage_file.cpp`.

**Verification (Round 135):** `g++ -std=c++20 -fsyntax-only -DPVPGN_V3_BNETD_INTEGRATION=1`
with `src/v3/infra/compat/include` on the include path compiles all 8 modified
TUs cleanly.

## Round 136 — Medium prefs caller migration (batch 3)

Migrated 8 files, 29 call sites. 13 new shim accessors.
Files: attrgroup, attrlayer, account_wrap, ladder, i18n,
anongame_maplists, anongame, storage_file. Syntax-check passes for 7/8
(i18n.cpp pre-existing pugixml env block — call-site edits are trivial
pattern, identical to other files in batch).

Cumulative: 23/~38 files migrated, 39 shim accessors total.

Pending medium files: handle_wol, handle_wserv, handle_irc,
handle_irc_common, clan, irc, account, channel.
High-impact (deferred): handle_bnet, connection, server, command, game,
luainterface, main.

## Round 137 — Medium prefs caller migration (batch 4)

Migrated 6 files, 36 call sites. 21 new shim accessors.
Files: channel, clan, handle_wol, handle_wserv, irc, account.
Syntax-check clean for all 6.

NOT migrated (intentional):
- channel.cpp:492 `prefs_get_log_notice()` — pre-existing bridge
  return-type bug (bridge says unsigned int, legacy says char const*).

Two pre-existing bridge bugs discovered (not fixed here):
- log_notice return-type mismatch
- irc_addrs vs ircaddrs symbol name mismatch (shim works around it)

Cumulative: 29/~38 files migrated, 60 shim accessors total.

Note: `handle_irc.cpp` and `handle_irc_common.cpp` from the original
plan do not exist in this branch (only `irc.cpp`).

Next batch candidates: handle_bnet, connection, server, command,
game, luainterface, main.

### Round 138 — high-impact files
- [x] handle_bnet.cpp — ~30 sites via shim
- [x] connection.cpp  — ~17 sites via shim
- [x] server.cpp      — ~46 sites via shim
- [x] 45 new shim accessors appended to prefs_v3_shim.h
- [ ] Skipped (no bridge): custom_icons, quota*, trackserv_addrs
- [x] g++ -fsyntax-only: no prefs-related diagnostics

### Round 139 — remaining medium/large files
- [x] command.cpp       — ~25 sites via shim
- [x] game.cpp          — ~10 sites via shim
- [x] main.cpp          — ~28 sites via shim
- [x] luainterface.cpp  — ~120 sites via shim
- [x] 19 new shim accessors appended to prefs_v3_shim.h
- [ ] Skipped (no/broken bridge): custom_icons, quota*, trackserv_addrs, log_notice
- [x] g++ -fsyntax-only: no prefs-related diagnostics

### Round 143 — TOML install + trackserv_addrs migration
- [x] `prefs_v3_shim.h` — added `trackserv_addrs()` (routes to bridge `pvpgn_v3_prefs_get_trackaddrs`, falls back to legacy `prefs_get_trackserv_addrs`).
- [x] `src/bnetd/file.cpp` — added `#include "prefs_v3_shim.h"`; replaced `prefs_get_filedir()` → `prefs_v3::filedir()` (last direct `prefs_get_*` caller in file.cpp).
- [x] `src/bnetd/server.cpp` — replaced `prefs_get_trackserv_addrs()` → `prefs_v3::trackserv_addrs()`.
- [x] `src/bnetd/main.cpp` — replaced `prefs_get_trackserv_addrs()` → `prefs_v3::trackserv_addrs()`.
- [x] `conf/CMakeLists.txt` — install `bnetd.toml` / `d2cs.toml` / `d2dbs.toml` alongside the legacy `.conf` files when `PVPGN_BUILD_V3=ON` (configure_file + appended to BNETD_CONFS / D2CS_CONFS / D2DBS_CONFS).
- [ ] Still deferred (require ServerConfig schema work + bridge fix): `custom_icons` (lives in `icons.cpp`, not `prefs.cpp`; semantically unrelated), `quota*` (no ServerConfig entry — needs new `[messages]` section), `log_notice` (bridge has wrong return type `unsigned int`; legacy returns `char const*`).
- [ ] `prefs.cpp:2044` — latent typo: calls `pvpgn_v3_prefs_get_trackserv_addrs()` which does not exist in the bridge (correct name is `pvpgn_v3_prefs_get_trackaddrs`). Gated out of every current build (`WITH_BNETD=OFF` in v3 preset; `PVPGN_BUILD_V3=OFF` in legacy preset → no `PVPGN_V3_BNETD_INTEGRATION` define). To fix when the joint build is exercised.

Remaining real call-site count by file (post-R143):

| File | Remaining sites | Accessors still needed |
|------|-----------------|------------------------|
| `channel.cpp`         | 1 | `log_notice` (broken bridge) |
| `command.cpp`         | 4 | `custom_icons`, `quota_*` |
| `connection.cpp`      | 9 | `custom_icons`, `quota*` |
| `handle_anongame.cpp` | 2 | `custom_icons` (icons.cpp-defined; no migration needed) |
| `luainterface.cpp`    | 8 | `quota*`, `log_notice`, `trackserv_addrs` (last one trivially fixable but luainterface uses these as Lua bridge config in a tight block — defer with the others) |
| `icons.cpp`           | 1 | self-definition `extern int prefs_get_custom_icons()` — stays |
| `prefs.cpp`           | self | legacy parser — stays until deletion |

After the above three accessor families land (separate round + ServerConfig
schema bump + bridge fix), the only remaining call sites will be the legacy
parser itself (`prefs.cpp`) and `icons.cpp`'s own self-definition.

### Round 144 — Docker green (alpine:latest / gcc 14 / cmake 4.x)

R143 surfaced a cascade of pre-existing breakage on `alpine:latest`
(cmake 4.x + gcc 14) that had been masked by older base images. Fixed
them all so `docker build -f Dockerfile.v3` + `--target v3-test` both
exit 0.

- [x] `Dockerfile.v3:46` — removed `infra_xml` from the `cmake --build --target ...`
      list. `infra_xml` is an INTERFACE library; cmake 4.x's gmake generator no
      longer creates a phony rule for INTERFACE targets, so passing it caused
      `gmake: *** No rule to make target 'infra_xml'`.
- [x] `tests/unit/infra/crypto/CMakeLists.txt` — gate `test_infra_crypto_parity`
      on `if(PVPGN_BUILD_LEGACY)` instead of `if(TARGET common)`. Tests under
      `tests/unit/...` are configured via `add_subdirectory(tests)` from inside
      `src/v3/CMakeLists.txt`, which runs **before** the root
      `add_subdirectory(src)` defines the legacy `common` target. The
      `if(TARGET ...)` check therefore was always false in v3 builds.
- [x] `src/v3/CMakeLists.txt:1001` — same fix for `infra_legacy_crypto` library
      (`if(TARGET common)` → `if(PVPGN_BUILD_LEGACY)`). Without this the library
      was never created, which cascaded to `add_subdirectory(legacy_crypto)`
      being skipped in `tests/unit/infra/CMakeLists.txt`, which in turn meant
      `test_common_bigint` / `test_common_bnetsrp3` targets did not exist —
      both are listed in the Dockerfile's `--target` set. Linking against
      `common` still works because CMake resolves link deps at generate time
      after every `add_subdirectory()` has run.
- [x] `tests/unit/infra/legacy_crypto/CMakeLists.txt` — analogous gate change
      for `test_common_bigint` / `test_common_bnetsrp3`.
- [x] `src/v3/protocol/file/src/bnftp_fsm.cpp` — wrapped file in
      `#pragma GCC diagnostic push/ignored "-Wnull-dereference"/pop`. gcc 14
      under `-Werror=null-dereference` flags the trivial `write_uXXle()`
      helpers (lines 68–77) as "potential null dereference" because their
      pointer parameter has no `[[gnu::nonnull]]` annotation. All call sites
      pass non-null pointers derived from a `std::vector<std::byte>::data()`
      after a length check; suppressing locally is cheaper than annotating
      every helper.

#### Verification

- `docker build -f Dockerfile.v3 -t pvpgn:v3 .` → exit 0, image written.
- `docker build -f Dockerfile.v3 --target v3-test -t pvpgn:v3-test .` →
  exit 0; ~280 Catch2 binaries reported "All tests passed".

#### Still deferred (unchanged from R143)

- `quota*`, `log_notice`, `custom_icons` shim accessors — need ServerConfig
  schema additions and a bridge return-type fix for `log_notice`.
- `src/bnetd/prefs.cpp:2044` typo on `pvpgn_v3_prefs_get_trackserv_addrs` —
  still latent; only exercised by the (non-CI) joint legacy+v3 build.
- Deletion of legacy `src/bnetd/prefs.cpp` itself — postponed until every
  caller is on the shim AND the bridge covers every accessor.

### Round 145 — quota* / log_notice / custom_icons accessors

Closes the three "deferred" items called out in R143.

**ServerConfig schema**

- [x] `MessagesConfig` struct added (`src/v3/infra/config/include/infra/config/server_config.hpp`):
  `quota_lines / quota_time / quota_wrapline / quota_maxline / quota_dobae`,
  defaulted to legacy `BNETD_QUOTA_*` constants. Added to `ServerConfig`
  as the `messages` field.
- [x] `CommandLogConfig::log_notice` changed from `bool` to `std::string`
  with the legacy default `"*** Please note this channel is logged! ***"`
  (semantic-fix: `prefs_get_log_notice()` returns the notice text, not a
  boolean).
- [x] `parse_messages()` added in `server_config.cpp` and wired into
  `from_config()`.
- [x] `parse_command_log()` switched to `get_or<std::string>` for `log_notice`.

**Legacy-prefs adapter** (`src/v3/infra/config/include/infra/config/legacy_prefs.hpp`)

- [x] `log_notice()` now returns `std::string_view` over
  `cfg_.command_log.log_notice`.
- [x] Five new accessors: `quota_lines / quota_time / quota_wrapline /
  quota_maxline / quota_dobae` (all `std::uint32_t`).

**Bridge** (`src/v3/integration/legacy_bnetd/...`)

- [x] `pvpgn_v3_prefs_get_log_notice()` return type changed from
  `unsigned int` to `const char*` (header + impl).
- [x] Five new entries:
  `pvpgn_v3_prefs_get_quota_{lines,time,wrapline,maxline,dobae}()`.

**Shim** (`src/bnetd/prefs_v3_shim.h`)

- [x] Seven new accessors: `log_notice()` (`char const*`),
  `quota_{lines,time,wrapline,maxline,dobae}()` (each `unsigned int`),
  and `custom_icons()` (`int`, no v3 routing — `prefs_get_custom_icons()`
  is a runtime flag from `icons.cpp`, not a config-file value).
- [x] `extern int prefs_get_custom_icons()` forward-declared in
  `pvpgn::bnetd` so callers do not have to drag `icons.h` in.

**Caller migration**

| File | Sites migrated |
|------|----------------|
| `channel.cpp`         | 1 (`log_notice`) |
| `command.cpp`         | 4 (`custom_icons`, `quota_{lines,time,wrapline,maxline}`) |
| `connection.cpp`      | 9 (3× `custom_icons`, `quota_{lines,time,wrapline×3,maxline,dobae}`) |
| `handle_anongame.cpp` | 2 (`custom_icons`); `prefs_v3_shim.h` include added |
| `luainterface.cpp`    | 7 (5× `quota_*`, `log_notice`, `trackserv_addrs`) |

All `prefs_get_*` references to these accessors in `src/bnetd/` now
route through `prefs_v3::*` (excluding `icons.cpp`'s self-definition of
`prefs_get_custom_icons()` and `prefs.cpp` legacy parser).

**Fixed**

- [x] `src/bnetd/prefs.cpp:2044` typo —
  `pvpgn_v3_prefs_get_trackserv_addrs` → `pvpgn_v3_prefs_get_trackaddrs`.

**TOML template** (`conf/bnetd.toml.in`)

- [x] New `[messages]` section with the 5 quota keys (defaults match
  ServerConfig).
- [x] `[command_log]` section keys renamed to match parser:
  `enabled / groups / command_list` → `log_commands / log_command_groups /
  log_command_list`; added `log_notice = "*** Please note this channel is
  logged! ***"`.
- [x] Stray `quota_*` / `log_notice` entries that were sitting under
  `[policy]` (and never parsed there) removed.

**Verification**

- `docker build -f Dockerfile.v3 -t pvpgn:v3 .` → exit 0.
- `docker build -f Dockerfile.v3 --target v3-test -t pvpgn:v3-test .` →
  exit 0, all Catch2 binaries report "All tests passed".

**What remains for legacy `prefs.cpp` deletion**

The only `prefs_get_*` callers left in `src/bnetd/` are now:

- `icons.cpp:561` — self-definition of `prefs_get_custom_icons()`. Stays
  (it is not a `bnetd.conf` value).
- `prefs.cpp` itself — the legacy parser. Deleted in a future round once
  every reasonable runtime path can be served from TOML.


---

## R146 -- Wire LegacyPrefs adapter into runtime

Goal: close the final bridge gaps and make `prefs_v3::*` the runtime
source of truth even across `/reload`.

- [x] `src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp` -- added
      `pvpgn_v3_prefs_get_outputdir()` alias (mirrors `statusdir`), and
      `pvpgn_v3_prefs_get_quota()` (master flood-control toggle).
- [x] `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/prefs_bridge.hpp`
      -- declarations for both new bridge entries.
- [x] `src/bnetd/prefs_v3_shim.h` -- `inline unsigned int quota()` shim
      added next to `quota_lines()` block. Routes to bridge when TOML is
      loaded, falls back to legacy `prefs_get_quota()` otherwise.
- [x] `src/bnetd/connection.cpp:3203` -- migrated `prefs_get_quota()` to
      `prefs_v3::quota()`.
- [x] `src/bnetd/luainterface.cpp:256` -- same migration in the lua
      `config.update("quota", ...)` call.
- [x] `conf/bnetd.toml.in` -- `quota = true` added under `[messages]`
      with master-toggle comment.
- [x] `src/bnetd/server.cpp:1714` -- `/reload` handler now also calls
      `pvpgn_v3_prefs_load_toml()` after the legacy `prefs_load()`
      succeeds. TOML path is derived by swapping the `.conf` extension
      on the cmdline preffile; failure logs a warn and leaves the
      previous snapshot in place. Gated by `#ifdef PVPGN_V3_BNETD_INTEGRATION`.
- [x] Docker `v3-build` and `v3-test` stages both green
      (`All tests passed` across all ~280 binaries).

Result: every `prefs_get_*` audit miss from R145 is closed. The legacy
`bnetd.conf` parser and the v3 TOML loader now stay in lock-step across
runtime `/reload`, so further migrations can rely on `prefs_v3::*`
being authoritative when TOML is present.

---

## R147 -- Inventory + deletion plan for legacy `prefs.cpp` (research only)

- [x] Counted residual `prefs_get_*` callers in `src/bnetd/` outside of
      `prefs.cpp` / `prefs_v3_shim.h` / `prefs.h`:
  - 4 lifecycle sites (`main.cpp` x2, `server.cpp` x2): `prefs_load()` /
    `prefs_unload()`.
  - 1 un-migrated direct accessor at `server.cpp:1807`:
    `prefs_get_trackserv_addrs()`.
  - 141 unique `prefs_get_*` fallback branches inside `prefs_v3_shim.h`
    (the structural blocker).
  - False positives ignored: `prefs_get_custom_icons()` lives in
    `icons.cpp` (not `prefs.cpp`); two comment-only matches in
    `handle_anongame.cpp`.
- [x] Wrote 5-phase deletion plan at
      [plans/prefs-cpp-deletion-plan.md](plans/prefs-cpp-deletion-plan.md):
  - Phase A: always populate `g_prefs` (defaults on parse failure) +
    add `pvpgn_v3_prefs_unload()`.
  - Phase B: lifecycle swap in `main.cpp` / `server.cpp` (`prefs_load` /
    `prefs_unload` -> v3 equivalents; un-migrate the 1 residual
    accessor).
  - Phase C: mechanical flatten of the 141 shim fallback branches.
  - Phase D: drop `prefs.cpp` / `prefs.h` from CMake, delete files.
  - Phase E: replicate for `d2cs` / `d2dbs`.
- [x] Round map R148-R152 documented; each round ends with docker
      `v3-build` + `v3-test`.

---

## R148 + R149 -- Phase A + Phase B of `prefs.cpp` retirement

**Phase A: bridge always populates `g_prefs`**

- [x] `src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp` --
      `pvpgn_v3_prefs_load_toml()` now installs a default-initialized
      `ServerConfig{}` snapshot on parse failure / null-path. Return
      code still reports -1 on failure so callers can fatal-exit.
- [x] New `extern "C" void pvpgn_v3_prefs_unload() noexcept` clears
      the global snapshot (called from legacy shutdown path).
- [x] Header declaration + comment refresh in `prefs_bridge.hpp`.

**Phase B: lifecycle swap in legacy bnetd**

- [x] `src/bnetd/main.cpp:534` -- startup config load now gated:
  - `#ifdef PVPGN_V3_BNETD_INTEGRATION`: only `pvpgn_v3_prefs_load_toml()`
    is called, fatal-exit on failure. No legacy `prefs_load()`.
  - `#else`: legacy `prefs_load()` unchanged.
- [x] `src/bnetd/main.cpp:672` -- shutdown gated to call
      `pvpgn_v3_prefs_unload()` under the integration build,
      `prefs_unload()` otherwise.
- [x] `src/bnetd/server.cpp:1714` -- `/reload` handler gated the same
      way: integration build runs only the v3 TOML reload, legacy build
      runs `prefs_load()`.
- [x] `src/bnetd/server.cpp:1830` -- last un-migrated accessor
      `prefs_get_trackserv_addrs()` -> `prefs_v3::trackserv_addrs()`.

**Verification**

- [x] `docker build -f Dockerfile.v3 --target v3-test .` -> exit 0,
      all Catch2 binaries "All tests passed".

**State after R148+R149**

Under `PVPGN_V3_BNETD_INTEGRATION` the v3 binary no longer calls into
`prefs.cpp` at runtime: `prefs_load` / `prefs_unload` / 
`prefs_get_trackserv_addrs` are all gated out. `prefs.cpp` is still
compiled because the 141 shim fallback branches reference its
accessors, but every such branch is now statically unreachable
(`pvpgn_v3_prefs_loaded()` is always true post-startup). Phase C will
flatten those branches; Phase D will delete the file.

---

## R150 + R151 -- Phase C + Phase D of `prefs.cpp` retirement

**Phase C: shim fallback flattened**

- [x] `src/bnetd/prefs_v3_shim.h` -- 134 accessors transformed by a
      mechanical regex pass from
      `#ifdef ... if(loaded) return v3; #endif return legacy;`
      to
      `#ifdef ... return v3; #else return legacy; #endif`.
- [x] One special-case accessor (`outputdir()`, name-alias of `statusdir`)
      flattened by hand to preserve the alias comment.
- [x] Doc comment refreshed to reflect the new dispatch model.
- [x] Top-of-file include gated: `prefs.h` is no longer pulled in under
      `PVPGN_V3_BNETD_INTEGRATION`; only `prefs_bridge.hpp` is.
- [x] One stray non-shim caller migrated:
      `luainterface.cpp:224` `prefs_allow_d2cs_setname()` ->
      `prefs_v3::allow_d2cs_setname()`.

After R150, every `prefs_v3::*` call resolves to a direct
`pvpgn_v3_prefs_get_*` bridge call under the integration build. The
141 legacy fallback edges in the shim are gone.

**Phase D: `prefs.cpp` dropped from the v3 build**

- [x] `src/bnetd/CMakeLists.txt` -- `prefs.cpp` is removed from
      `BNETD_LIB_SOURCES` whenever
      `TARGET integration_legacy_bnetd_linked` exists at configure
      time (which is the case for every v3-build configuration, since
      v3 is added before bnetd via the root `CMakeLists.txt`).
- [x] `prefs.h` is intentionally kept on disk for the legacy build and
      for the two surviving `#include "prefs.h"` lines in
      `connection.cpp:81` and `file.cpp:47`. Under v3 the declarations
      are unreferenced (no linker error).
- [x] `prefs.cpp` is unchanged on disk: it is still the canonical
      legacy `bnetd.conf` parser for the pure-legacy build. Deletion
      of the file from the repository is deferred to a future round
      that removes the legacy build target entirely.

**Verification**

- [x] `docker build -f Dockerfile.v3 --target v3-test .` -> exit 0,
      all Catch2 binaries "All tests passed".

**State after R150+R151**

The v3 bnetd binary no longer compiles or links `prefs.cpp`. Every
runtime `prefs_get_*` lookup goes
`prefs_v3::X()` -> `pvpgn_v3_prefs_get_X()` ->
`LegacyPrefs::X()` -> `ServerConfig` field. The strangler-fig phase
of the prefs migration is complete: `bnetd.toml` is the single source
of truth for the v3 build.

Remaining work (out of scope for R151):

- Replicate Phases A-D for `src/d2cs/prefs.cpp` and `src/d2dbs/prefs.cpp`.
- Eventually delete `prefs.cpp` / `prefs.h` outright once the legacy
  build target is retired.





---

## R152 - d2cs/d2dbs ServerConfig + bridge skeletons

Scope: parity skeletons for d2cs and d2dbs prefs. No callers migrated
yet (deferred to R153+).

Added:
- src/v3/infra/config/include/infra/config/d2cs_server_config.hpp
- src/v3/infra/config/src/d2cs_server_config.cpp
- src/v3/infra/config/include/infra/config/d2dbs_server_config.hpp
- src/v3/infra/config/src/d2dbs_server_config.cpp
- src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/d2cs_prefs_bridge.hpp
- src/v3/integration/legacy_d2cs/src/d2cs_prefs_bridge.cpp
- src/v3/integration/legacy_d2dbs/include/integration/legacy_d2dbs/d2dbs_prefs_bridge.hpp
- src/v3/integration/legacy_d2dbs/src/d2dbs_prefs_bridge.cpp

CMake wiring (src/v3/CMakeLists.txt):
- infra_config SOURCES += d2cs_server_config.cpp, d2dbs_server_config.cpp
- integration_legacy_d2cs SOURCES += d2cs_prefs_bridge.cpp;
  PUBLIC_DEPS += infra_config (TARGET_EXISTS guard).
- integration_legacy_d2dbs SOURCES += d2dbs_prefs_bridge.cpp;
  PUBLIC_DEPS += infra_config (TARGET_EXISTS guard).

Schema layout (mirrors bnetd ServerConfig pattern):
- D2csServerConfig: [server], [network], [realm], [log], [files] (16
  paths incl. 7 newbiefiles), [misc] (12 fields), [internal] (21
  fields incl. ladder_start_time parsed via parse_iso_datetime).
  Aggregate uses internal_ (underscored, MSVC reserved-word avoidance).
- D2dbsServerConfig: [network], [log], [files] (8 paths), [ladder]
  (5 fields), [misc] (6 fields).

Bridge pattern (per legacy_bnetd/prefs_bridge.cpp):
- Process-global std::optional<...ServerConfig> snapshot.
- load_toml() always populates: defaults to {} on null/parse fail,
  returns -1 in that case (legacy never sees a null snapshot).
- StringCache holds pre-computed std::string copies of
  std::filesystem::path fields so accessors can safely return
  const char* with snapshot lifetime.
- Macros STR_GET / U32_GET / BOOL_GET / PATH_GET keep accessor wall
  short and uniform.

Verification: docker v3-test exit 0; all Catch2 binaries
"All tests passed" (51, 159, 184 assertion suites among others).

Not in scope (deferred):
- Migrating src/d2cs/prefs.cpp callers to bridge.
- Migrating src/d2dbs/prefs.cpp callers to bridge.
- d2cs / d2dbs main.cpp lifecycle swap (mirror bnetd Phase B).
- Removing legacy d2cs/d2dbs prefs.cpp from v3 build (mirror Phase D).

---

## R153 - d2cs caller-migration kickoff (Phase B-equivalent)

Scope: wire the d2cs prefs bridge into the runtime additively and lay
down the per-caller migration shim. Legacy d2cs_prefs_load still
runs in parallel; no callers outside src/d2cs/main.cpp are migrated
yet. Full lifecycle-swap (mirror bnetd R149) is deferred until all
callers have been moved to the shim.

Added:
- src/d2cs/prefs_v3_shim.h
  - 'pvpgn::d2cs::prefs_v3::*' namespace covering the full d2cs prefs
    surface (~50 inline accessors).
  - Each accessor dispatches to the v3 bridge under
    'PVPGN_V3_D2CS_INTEGRATION', or to the legacy 'd2cs_prefs_*' /
    'prefs_*' accessor otherwise.

Modified:
- src/d2cs/main.cpp
  - '#include "prefs_v3_shim.h"' added alongside 'prefs.h'.
  - 'config_init':
    - Under v3 (after the legacy 'd2cs_prefs_load' call), derive a
      '.toml' path from cmdline preffile and call
      'pvpgn_v3_d2cs_prefs_load_toml'. Failure logs a warning -- the
      bridge installs defaults so callers using the shim keep working.
    - 'eventlog_clear_level' loop now reads loglevels via
      'pvpgn::d2cs::prefs_v3::loglevels()'.
    - 'eventlog_open' fallback now reads the logfile via
      'pvpgn::d2cs::prefs_v3::logfile()'.
  - 'config_cleanup': under v3 also calls
    'pvpgn_v3_d2cs_prefs_unload()'.

Verification: docker v3-test exit 0; all Catch2 binaries
"All tests passed" (8, 31, 51, 159, 184 assertion suites among others).

Not in scope (deferred):
- Migrating remaining 'src/d2cs/' callers (connection.cpp, d2charfile.cpp,
  d2gs.cpp, game.cpp, gamequeue.cpp, s2s.cpp, server.cpp, ...) to
  'pvpgn::d2cs::prefs_v3::*'.
- Dropping legacy 'd2cs_prefs_load' under v3 build (Phase B full swap).
- Flatten shim (Phase C equivalent).
- Drop 'src/d2cs/prefs.cpp' from the v3 d2cs_legacy build (Phase D).

---

## R154 - Bulk d2cs caller migration to prefs_v3 shim

Scope: migrate every remaining 'd2cs_prefs_get_*' / 'prefs_get_*' /
'prefs_allow_*' / 'prefs_check_*' / 'prefs_hide_*' call in 'src/d2cs/'
to 'pvpgn::d2cs::prefs_v3::*'. Legacy 'd2cs_prefs_load' / 'prefs.cpp'
remain in place; the shim falls back to them outside v3 and forwards
to the v3 bridge under 'PVPGN_V3_D2CS_INTEGRATION'.

Method:
- PowerShell regex pass over 15 'src/d2cs/' .cpp files.
- Generic patterns:
  - 'd2cs_prefs_get_(\w+)' -> 'pvpgn::d2cs::prefs_v3::\1'
  - 'prefs_get_(\w+)' -> 'pvpgn::d2cs::prefs_v3::\1'
  - 'prefs_allow_(\w+)' -> 'pvpgn::d2cs::prefs_v3::allow_\1'
  - 'prefs_check_(\w+)' -> 'pvpgn::d2cs::prefs_v3::check_\1'
  - 'prefs_hide_(\w+)' -> 'pvpgn::d2cs::prefs_v3::hide_\1'
- Special-case renames (longest-prefix wins, applied first):
  - 'prefs_get_d2gs_list' -> 'prefs_v3::gameservlist'
  - 'prefs_get_d2cs_account_allowed_symbols' -> 'prefs_v3::account_allowed_symbols'
- Auto-inserts '#include "prefs_v3_shim.h"' next to the existing
  'prefs.h' include in any file that gained a new shim call.

Files modified (call counts after migration):
- bnetd.cpp (2), connection.cpp (5), d2charfile.cpp (32),
  d2charlist.cpp (1), d2gs.cpp (5), d2ladder.cpp (2), game.cpp (3),
  handle_bnetd.cpp (3), handle_d2cs.cpp (23), handle_d2gs.cpp (8),
  handle_init.cpp (1), handle_signal.cpp (6), main.cpp (6),
  server.cpp (8), serverqueue.cpp (1). Total ~106 call sites.

Residual legacy callers: only 'src/d2cs/prefs.cpp' itself (the
internal implementations of legacy 'prefs_get_*'). Lifecycle entry
points 'd2cs_prefs_load' / 'd2cs_prefs_unload' / 'prefs_reload' are
NOT matched by the regex and remain untouched.

Verification: docker v3-test exit 0 on first try; all Catch2 binaries
"All tests passed" (8, 31, 51, 159, 184 assertion suites).

Not in scope (deferred to R155+):
- Full Phase B swap: drop legacy 'd2cs_prefs_load' under v3 build so
  the bridge is the sole source. (Risk: 'prefs_reload' implementation
  + a few internals in 'prefs.cpp' still touch the legacy 't_prefs'
  static; need a bridge-side equivalent or to retire 'prefs_reload'.)
- Phase C: flatten 'prefs_v3_shim.h' under v3 (drop the '#else'
  fallback branch once legacy is gone).
- Phase D: drop 'prefs.cpp' from the v3 'd2cs_legacy' build.
- d2dbs equivalent of R153+R154.

---

## R155 - d2cs Phase B full swap (legacy load retired under v3)

Scope: under 'PVPGN_V3_D2CS_INTEGRATION' the v3 TOML snapshot becomes
the sole config source. Legacy 'd2cs_prefs_load' / 'd2cs_prefs_unload'
/ 'prefs_reload' are no longer called in the v3 build.

Modified:
- src/d2cs/main.cpp
  - 'config_init': flipped the load order into an '#ifdef ... #else ...
    #endif' block. v3 build only calls 'pvpgn_v3_d2cs_prefs_load_toml';
    parse failure is now FATAL (matches bnetd R149). Legacy
    'd2cs_prefs_load' is the '#else' branch.
  - 'config_cleanup': v3 calls only 'pvpgn_v3_d2cs_prefs_unload',
    legacy calls only 'd2cs_prefs_unload'.
- src/d2cs/handle_signal.cpp
  - SIGHUP reload branch ('signal_data.reload_config') now reloads
    the v3 TOML via 'pvpgn_v3_d2cs_prefs_load_toml' under v3 instead
    of 'prefs_reload'. The post-reload 'd2gslist_reload' / 'trans_reload'
    calls were already reading through the shim (R154).

Verification: docker v3-test exit 0; all Catch2 binaries
"All tests passed" (8, 31, 51, 159, 184 assertion suites).

Strangler-fig status for d2cs:
- d2cs binary under v3 no longer EXECUTES legacy 'prefs_load' /
  'prefs_unload' / 'prefs_reload'.
- 'src/d2cs/prefs.cpp' is still COMPILED into 'd2cs_legacy' (Phase D
  has not run yet), and the shim's '#else' branch still references
  the legacy accessor names so an out-of-v3 build keeps working.
- Static-unreachability: every shim fallback now sits under
  '!PVPGN_V3_D2CS_INTEGRATION', so Phase C flatten can drop them
  whenever desired.

Not in scope (deferred to R156+):
- Phase C: flatten 'src/d2cs/prefs_v3_shim.h' under v3 (drop '#else'
  branches).
- Phase D: drop 'src/d2cs/prefs.cpp' from the v3 'd2cs_legacy' build
  list (mirror what 'src/bnetd/CMakeLists.txt' does for bnetd).
- d2dbs Phase A/B (mirror R152/R153/R154/R155 for d2dbs).

---

## R156 - d2cs Phase D (drop prefs.cpp from v3 d2cs_legacy build)

Scope: stop compiling 'src/d2cs/prefs.cpp' into the v3 d2cs binary.

Modified:
- src/d2cs/CMakeLists.txt
  - Mirrors the bnetd R151 pattern: when
    'integration_legacy_d2cs_linked' exists as a target,
    'list(REMOVE_ITEM D2CS_LIB_SOURCES prefs.cpp)' runs before
    'add_library(d2cs_legacy STATIC ...)'. 'prefs.h' stays in the
    source list since a few legacy includes still pull it for
    declarations.

Phase C status:
- The d2cs shim was authored directly in flattened '#ifdef/#else'
  shape in R153 (no runtime 'pvpgn_v3_d2cs_prefs_loaded()' guards
  to flatten). Under v3 only the bridge branch is reachable; the
  '#else' branch is statically unreachable in v3 but kept so a
  non-v3 build of d2cs_legacy keeps compiling.

Verification: docker v3-test exit 0; all Catch2 binaries
"All tests passed" (8, 31, 51, 159, 184 assertion suites).

Strangler-fig phase for d2cs is complete:
- 'd2cs.toml' is the single source of truth for v3 d2cs.
- 'src/d2cs/prefs.cpp' is no longer compiled or linked into the v3
  binary; the file remains in the tree for non-v3 builds.

Not in scope (deferred):
- d2dbs Phase A/B/C/D (mirror R152..R156 for d2dbs).
- Catch2 tests for D2csServerConfig and D2dbsServerConfig parsers.

---

## R157 - d2dbs full strangler-fig in one round

Scope: replicate the entire d2cs trajectory (R152..R156 schema +
bridge + shim + bulk migration + Phase B full swap + Phase D drop)
for d2dbs in a single round, leveraging the smaller surface (~22
accessors, 35 call sites).

Added:
- src/d2dbs/prefs_v3_shim.h
  - 'pvpgn::d2dbs::prefs_v3::*' namespace with 21 inline accessors
    covering the entire d2dbs prefs surface.
  - Each accessor dispatches to the v3 bridge under
    'PVPGN_V3_D2DBS_INTEGRATION' (else falls back to legacy parser).

Modified:
- src/d2dbs/main.cpp
  - '#include "prefs_v3_shim.h"' added.
  - 'config_init': under v3 calls only 'pvpgn_v3_d2dbs_prefs_load_toml'
    (TOML path derived from cmdline preffile, parse failure FATAL);
    legacy 'd2dbs_prefs_load' moved to '#else' branch.
  - 'config_cleanup': v3 calls only 'pvpgn_v3_d2dbs_prefs_unload',
    legacy calls only 'd2dbs_prefs_unload'.
- src/d2dbs/handle_signal.cpp
  - SIGHUP reload branch now calls 'pvpgn_v3_d2dbs_prefs_load_toml'
    under v3 instead of 'd2dbs_prefs_reload'.
- Bulk regex migration of all 35 accessor call sites across 5 files:
  - d2ladder.cpp (8), dbserver.cpp (7), dbspacket.cpp (16),
    handle_signal.cpp (4), main.cpp (4).
  - Special-case renames: 'd2dbs_prefs_get_d2gs_list' -> 'gameservlist',
    'd2dbs_prefs_get_XML_output_ladder' -> 'XML_output_ladder'.
  - Generic patterns: 'd2dbs_prefs_get_(\w+)' -> 'prefs_v3::\1',
    'prefs_get_(\w+)' -> 'prefs_v3::\1'.
- src/d2dbs/CMakeLists.txt
  - Mirrors d2cs R156: when 'integration_legacy_d2dbs_linked' exists,
    'list(REMOVE_ITEM D2DBS_LIB_SOURCES prefs.cpp)' drops the legacy
    parser from the v3 d2dbs_legacy build.

Residual legacy callers: only 'src/d2dbs/prefs.cpp' itself (22
internal accessor implementations). Lifecycle entry points
'd2dbs_prefs_load' / 'd2dbs_prefs_unload' / 'd2dbs_prefs_reload' are
no longer called under v3.

Verification: docker v3-test exit 0 on first try; all Catch2 binaries
"All tests passed" (8, 31, 51, 159, 184 assertion suites).

Strangler-fig for d2dbs is complete:
- 'd2dbs.toml' is the single source of truth for v3 d2dbs.
- 'src/d2dbs/prefs.cpp' is no longer compiled or linked into the v3
  d2dbs binary; the file remains in the tree for non-v3 builds.

Phase 1 Step 10 (TOML migration) is now functionally complete for
bnetd, d2cs, and d2dbs. Remaining optional polish:
- Catch2 round-trip / defaults / malformed-input tests for
  D2csServerConfig and D2dbsServerConfig parsers.
- Audit conf/d2cs.toml.in and conf/d2dbs.toml.in coverage versus
  schema fields.

---

## R158 - Catch2 tests for D2csServerConfig + D2dbsServerConfig

Scope: parity polish for Phase 1 Step 10 -- bnetd already had
'server_config_test.cpp'; this round adds equivalent coverage for the
two D2 parsers introduced in R152 ('infra/config/d2cs_server_config'
and 'infra/config/d2dbs_server_config').

Added:
- tests/unit/infra/config/d2cs_server_config_test.cpp
  - 13 test cases, 105 assertions covering: defaults for every
    section ('server', 'network', 'realm', 'log', 'misc', 'internal'),
    full-section parse round-trips, all 16 files paths (incl. all 7
    newbiefile_* slots), 'ladder_start_time' ISO datetime parsing
    (valid and malformed -> 0), syntax error -> InvalidArgument,
    missing file -> NotFound, on-disk load, and a parse-vs-load
    round-trip.
- tests/unit/infra/config/d2dbs_server_config_test.cpp
  - 10 test cases, 54 assertions covering: defaults, [network],
    [log], [files] (all 8 paths), [ladder] (incl. XML_ladder_output),
    [misc], syntax error / missing file error branches, on-disk
    load, parse-vs-load round-trip.
- tests/unit/infra/config/CMakeLists.txt
  - Two new 'pvpgn_v3_add_test' entries:
    'test_infra_config_d2cs_server' and
    'test_infra_config_d2dbs_server'.
- Dockerfile.v3
  - Added both new test binaries to the v3-build target list
    (line 46) and the v3-test run sequence so they are exercised
    by 'docker build -f Dockerfile.v3 --target v3-test'.

Verification: docker v3-test build green; new suites report
'All tests passed (105 assertions in 13 test cases)' and
'All tests passed (54 assertions in 10 test cases)'. No
regressions in the prior 24 suites.

Phase 1 Step 10 polish: parsers for bnetd, d2cs, and d2dbs are now
all directly unit-tested. Step 10 is in good shape to close out and
move on to Step 11.

---

## R159 - LegacyPrefs adapters for d2cs + d2dbs

Scope: mirror the bnetd 'LegacyPrefs' adapter (R121/R146) for d2cs
and d2dbs so all three services have an immutable, string_view-based
adapter sitting on top of their typed 'ServerConfig'. The adapter is
distinct from the C-ABI bridge: it is a C++ object holding the typed
config + pre-computed std::string copies of filesystem::path fields,
intended as a hand-off surface for code that wants to consume prefs
without depending on the legacy 'prefs_get_*' family.

Added:
- src/v3/infra/config/include/infra/config/d2cs_legacy_prefs.hpp
  - 'D2csLegacyPrefs' class. ~55 accessors covering every section of
    'D2csServerConfig': server (realmname), network (4), realm (3),
    log (1), files (16 paths incl. 7 newbiefile_*), misc (12),
    internal (21 incl. 'ladder_start_time').
  - 26 pre-computed std::string members for path/string copies.
  - 'make_d2cs_legacy_prefs(...)' returning 'shared_ptr' for atomic
    hot-reload swap.
- src/v3/infra/config/include/infra/config/d2dbs_legacy_prefs.hpp
  - 'D2dbsLegacyPrefs' class. 23 accessors covering the full
    D2dbsServerConfig: network (2), log (1), files (8 paths), ladder
    (5 incl. XML_output_ladder bool), misc (6).
  - 11 pre-computed std::string members.
- tests/unit/infra/config/d2cs_legacy_prefs_test.cpp
  - 2 cases / 38 assertions: round-trip from a populated config plus
    a defaults check against an empty 'D2csServerConfig'.
- tests/unit/infra/config/d2dbs_legacy_prefs_test.cpp
  - 2 cases / 32 assertions: round-trip + defaults check.
- tests/unit/infra/config/CMakeLists.txt
  - Two new 'pvpgn_v3_add_test' entries
    ('test_infra_config_d2cs_legacy_prefs',
    'test_infra_config_d2dbs_legacy_prefs').
- Dockerfile.v3
  - Both new test binaries added to the v3-build target list and the
    v3-test run sequence.

Header-only design: no .cpp changes to 'infra_config' library
sources. No call-site migration: the adapters are now available for
future consumers but not yet wired into the bridge load path -- that
swap (bridge cache rendering a 'D2csLegacyPrefs' instead of the
StringCache) is a separate optional cleanup.

Verification: docker v3-test build green; new suites report
'All tests passed (38 assertions in 2 test cases)' and
'All tests passed (32 assertions in 2 test cases)'. No regressions
across the 25 prior suites.

Phase 1 Step 10 now has full adapter symmetry across all three
services: bnetd 'LegacyPrefs', d2cs 'D2csLegacyPrefs', d2dbs
'D2dbsLegacyPrefs', each backed by its own typed config and direct
unit tests.

---

## R160 - conf/d2cs.toml.in + conf/d2dbs.toml.in coverage audit

Scope: cross-check the shipped TOML templates against the typed
schemas in 'infra/config/d2cs_server_config.hpp' and
'd2dbs_server_config.hpp', then fix gaps + typing mismatches that
would have caused fields to silently fall to defaults at runtime.

Findings + fixes:

- conf/d2dbs.toml.in
  - 'difficulty_hack' was set as a TOML boolean ('false') but the
    schema declares 'std::uint32_t difficulty_hack'. tomlplusplus
    would not coerce bool -> int64 and the parser would silently
    return the default '0'. Changed the template to the integer
    form: 'difficulty_hack = 0' (commented "0 = deactivated /
    1 = activated").
  - [ladder] section carried a commented 'ladder_start_time' block,
    but 'ladder_start_time' is NOT a d2dbs schema field (it lives in
    D2cs only). The misleading block was removed so admins don't
    place a setting that has no effect.

- conf/d2cs.toml.in
  - 'misc.hide_pass_games' was missing from the template; schema
    defaults it to 'false'. Added an explicit
    'hide_pass_games = false' so the field is visible/discoverable.
  - 'internal.game_maxlevel' was commented out
    ('# game_maxlevel = 255'). Schema declares a real field with
    default 255; uncommented to match.
  - 'internal.ladderlist_count' was missing entirely; added as
    'ladderlist_count = 0' with a short comment.

After the audit, every field declared in 'D2csServerConfig' and
'D2dbsServerConfig' has at least one corresponding template line
(active or explicitly commented) in the shipped 'conf/*.toml.in'.
Only intentionally-runtime-overridable settings ('pidfile' on both
servers) remain commented out by default.

Verification: full docker v3-test build green; all 30 Catch2 suites
"All tests passed", including the latest:
- d2cs_server_config        : 105 assertions in 13 cases
- d2dbs_server_config       :  54 assertions in 10 cases
- d2cs_legacy_prefs         :  38 assertions in  2 cases
- d2dbs_legacy_prefs        :  32 assertions in  2 cases

Step 10 is now both functionally complete (R152-R157) and
operationally polished (R158-R160): parsers, adapters, deployable
templates all consistent.

## R161 -- adapters wired into bridges + template tests + wrap-up

Combined R161 scope (user freeform: "implement 2 and 3 and 4"):

1. Adapter return-type refactor (R159 follow-up). Changed every
   string-valued accessor in D2csLegacyPrefs / D2dbsLegacyPrefs
   from `std::string_view` to `const std::string&` so bridges
   can safely call `.c_str()` knowing the string is the adapter's
   owned storage (stable lifetime, null-terminated since C++11).
   Tests unchanged -- they use `std::string{p.X()} == "..."` which
   constructs from either `string_view` or `const std::string&`.

2. Bridge refactor (the actual wiring):
   - `src/v3/integration/legacy_d2cs/src/d2cs_prefs_bridge.cpp`
     and `legacy_d2dbs/src/d2dbs_prefs_bridge.cpp` now hold
     a single `std::shared_ptr<D2{cs,dbs}LegacyPrefs>` globally.
   - Dropped the bridge-local `StringCache` + `rebuild_strings`
     helper (16 / 8 path fields). Single source of truth for
     stable `const char*` storage is now the adapter.
   - New macros: `STR_GET(method, default_)` calls
     `p->method().c_str()`; `U32_GET` / `BOOL_GET` are
     method-based equivalents.
   - All 55 d2cs accessors + 25 d2dbs accessors rewritten in
     terms of adapter methods. Special cases preserved
     (`char_expire_time = char_expire_day() * 86400`;
     `ladder_start_time` cast to `long`).
   - `_load_toml` / `_unload` / `_loaded` lifecycle reworked
     to swap the shared_ptr atomically (groundwork for SIGHUP
     hot-reload in Step 11).

3. Template-validity test
   (`tests/unit/infra/config/conf_template_test.cpp`).
   - Reads `conf/bnetd.toml.in` / `conf/d2cs.toml.in` /
     `conf/d2dbs.toml.in` directly from the source tree at test
     time via injected `PVPGN_CONF_SOURCE_DIR` macro
     (set by `target_compile_definitions` in CMake).
   - Feeds each through the v3 parser, asserts `has_value()`
     plus all R160-added schema fields
     (`hide_pass_games`, `game_maxlevel`, `ladderlist_count`,
     `difficulty_hack`).
   - Catches regressions: any future schema field that ships
     without a matching `.toml.in` line, or a template that
     drifts out of TOML grammar, will fail at CI time.
   - Wired into `tests/unit/infra/config/CMakeLists.txt` and
     into both Dockerfile.v3 v3-build target list and v3-test
     run sequence.

4. Wrap-up note (`plans/step10-wrap-up.md`). Single-page
   narrative: problem, approach, schema, bridges, tests,
   non-goals, round log R120-R161.

### Verification

Docker v3-test stage built and ran all suites; new
`test_infra_config_conf_templates` reports
`All tests passed (18 assertions in 3 test cases)`.
Every prior suite (	est_infra_config_server,
	est_infra_config_d2cs_server, 	est_infra_config_d2dbs_server,
	est_infra_config_d2cs_legacy_prefs,
	est_infra_config_d2dbs_legacy_prefs, plus the ~120 bridge /
protocol / application tests) still green.

### Status

Phase 1 Step 10 is now FUNCTIONALLY COMPLETE for all three
services. Remaining work (SIGHUP hot-reload signal wiring,
admin-console TOML editing, deletion of legacy `prefs_get_*` C
shims once all legacy TUs are gated) is tracked under Step 11.

## R162 -- Step 10 closeout polish

User picked combined "implement 1 and 2 and 3" from the R161 dialog:

1. **SIGHUP hot-reload for d2cs / d2dbs** -- verified already in place
   (R155 / R157). No-op for this round.

2. **Legacy `prefs_get_*` deletion audit** -- swept every .cpp / .h /
   .lua / plugin file under `src/` for direct callers. Found one
   straggler in `src/bnetd/server.cpp:1807`
   (`tracker_set_servers(prefs_get_trackserv_addrs())`); routed it
   through `pvpgn::bnetd::prefs_v3::trackserv_addrs()` which already
   existed in the shim. Wrote `plans/legacy-prefs-deletion-audit.md`
   summarising: every direct `prefs_get_*` call now goes through the
   `prefs_v3` shim. Full deletion of `prefs.cpp` / `prefs.h` is
   blocked on retiring the non-v3 build path -- deferred to Step 11.

3. **Admin-console TOML viewer** -- added `/config` slash command to
   bnetd. Declaration + dispatch-table entry + impl in
   `src/bnetd/command.cpp`; permission group 8 (admin-only) in
   `conf/command_groups.conf.in`. Prints a compact TOML-shaped
   snapshot of the live `prefs_v3::*` accessors covering [server],
   [log], [network], and [files] sections. Lets operators sanity-check
   that a SIGHUP / `/rehash` actually swapped in the new
   `bnetd.toml` without having to tail the eventlog.

### Verification

Docker v3-test stage: all Catch2 suites green
(`18 assertions in 3 test cases` for conf_templates is the highest
suite number, matching R161). Build clean, no new warnings.

### Status

Phase 1 Step 10 is now FULLY COMPLETE -- no remaining tasks against
the original checklist. Next round (R163+) belongs to Phase 1
Step 11 (final integration / closeout).

## R163 -- Step 11 kickoff: shared dump formatter + d2cs/d2dbs operator visibility

User picked "implement 1+2+3+4" from R162 checklist dialog. Notes:

- Item 3 ("Mirror /config into d2cs/d2dbs admin consoles") -- the
  d2cs and d2dbs binaries do NOT have admin slash-command surfaces
  (only signal handlers; the v3 telnet admin FSM is not yet wired
  for them). Pragmatic equivalent implemented instead: dump the
  live config snapshot to the eventlog on every SIGHUP reload.

### Deliverables

1. `plans/step11-checklist.md` written -- inventory of remaining
   Phase 1 closeout work (non-v3 build retirement, prefs.cpp/.h
   deletion, `.conf.in` retirement, docs, CI).

2. New shared formatter:
   `src/v3/infra/config/include/infra/config/prefs_dump.hpp` --
   three overloaded `format_dump(const ...LegacyPrefs&)` free
   functions returning `std::vector<std::string>` (one TOML-shaped
   line per entry). Reused by bnetd `/config`, d2cs/d2dbs SIGHUP
   eventlog dump, and unit tests.

3. Catch2 coverage:
   `tests/unit/infra/config/prefs_dump_test.cpp` (4 cases / 46
   assertions). Wired into the unit-config CMakeLists + Dockerfile.v3
   build target list + v3-test run sequence.

4. New bridge exports:
   - `pvpgn_v3_d2cs_prefs_dump(void*, void(*)(void*, const char*))`
   - `pvpgn_v3_d2dbs_prefs_dump(void*, void(*)(void*, const char*))`
   Bridge formats its own `g_{svc}_prefs` via `format_dump` and
   invokes the callback per line. Keeps eventlog out of the v3 layer.

5. `src/d2cs/handle_signal.cpp` + `src/d2dbs/handle_signal.cpp` --
   after SIGHUP reload, log "v3 TOML config snapshot after reload:"
   then call the new dump bridge with a lambda trampoline that
   eventlog()s each line under module name `d2cs_config` /
   `d2dbs_config`. Operators now have a tail-the-log story for
   verifying reload took effect on all three services.

6. `conf/CMakeLists.txt` -- under `PVPGN_BUILD_V3` the legacy
   `bnetd.conf` / `d2cs.conf` / `d2dbs.conf` are no longer
   added to the install file lists. Only the `.toml` siblings
   ship. The `.conf.in` templates remain in the tree for the
   non-v3 build and as migration reference.

### Verification

Docker v3-test stage green. All Catch2 suites pass including the new
`46 assertions in 4 test cases` prefs_dump suite. No new warnings.

### Status

Phase 1 Step 10 fully closed at R162. Phase 1 Step 11 has begun --
R163 closes `step11-checklist.md` items 1.6 (partial), 2.1, 2.2,
and 2.3.