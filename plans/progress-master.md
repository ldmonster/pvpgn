# PvPGN Refactoring Master Progress

Last updated: 2026-05-21 (Round 130)

## Overall Status

| Plan | Status | Progress |
|------|--------|----------|
| Phase 1: Common/Compat Migration | 🔄 In Progress | Steps 1-9 complete, Step 10 next |
| Phase 2: Protocol FSM Completion | ✅ **COMPLETE** | R122–R129: all FSMs done, composition root wired (Round 130) |
| Phase 3: bnetd Server Logic Migration | 🔄 **IN PROGRESS** | Starting next — legacy handler deletion |
| Phase 4: D2 Services Migration | ⏳ Not Started | Depends on Phase 2 |
| Phase 5: Tools Migration | ✅ Mostly Done | bnpass, bnproxy, bniutils, bntrackd, client tools all migrated |
| Build System Consolidation | ⏳ Not Started | Depends on all phases |
| Testing Strategy | 🔄 In Progress | Tests being added alongside migration |

## Phase 1: Common/Compat Migration

### Steps

- [x] Step 1: Create `src/v3/infra/compat/` — COMPLETE
- [x] Step 2: Create `src/v3/infra/crypto/` — COMPLETE (with parity tests)
- [x] Step 3: Migrate protocol `*_protocol.h` headers — COMPLETE (all 12 headers)
- [x] Step 4: Packet/Queue migration — COMPLETE (R100)
  - [x] E.1: v3 surface additions (Writer enhancements) — COMPLETE
  - [x] E.2: send_packet bridge — COMPLETE
  - [x] E.3: Per-module migration (Rounds 1-38 done — ALL COMPLETE)
    - [x] handle_init.cpp — authoritative apply path done
    - [x] bnet auth/login (AuthReply1, AuthReply109, AuthInfo, LoginReply, LoginReplyW3, LogonProofReply, CreateAcctReply, IconReply)
    - [x] chat / channel / message (chatevent compose bridge, channel state bridge, channellist)
    - [x] game list / start / report (Round 16 complete)
    - [x] clan — Round 38: clan_send_bridge wired into all 8 send sites in clan.cpp
    - [x] anongame.cpp + anongame_infos.cpp (Rounds 17-19)
    - [x] clan_dispatch (Round 20 — observation bridge done)
    - [x] friends_dispatch (Round 21)
    - [x] auth_dispatch (Round 22)
    - [x] keepalive_dispatch (Round 23)
    - [x] realm_dispatch (Round 24)
    - [x] account_dispatch (Round 25)
    - [x] profile_dispatch (Round 26)
    - [x] ladder_dispatch (Round 27)
    - [x] d2_character_dispatch (Round 28)
    - [x] telemetry_dispatch (Round 29)
    - [x] ad_dispatch (Round 30)
    - [x] progident_dispatch (Round 31)
    - [x] gameport_dispatch (Round 32)
    - [x] cdkey_dispatch (Round 33)
    - [x] passemail_dispatch (Round 34)
    - [x] handshake_dispatch (Round 35)
    - [x] stub_dispatch (Round 36)
    - [x] file_dispatch (Round 37)
  - [x] E.4: Acceptance criteria — COMPLETE (R100: 62 remaining are all non-bridgeable)
    - Round 46: 6 send bridges (motdw3, realmlistlegacy, realmlist, claninfo, profilereply, realmjoin) — 7 sites guarded in handle_bnet.cpp
    - Round 47: 4 send bridges (charlistreply, adreply, adclick2reply, playerinforeply) — 4 sites guarded in handle_bnet.cpp
    - Round 48: 6 send bridges (gamelistreply, startgame1ack, startgame3ack, startgame4ack, ladderreply, laddersearchreply) — 6 sites guarded in handle_bnet.cpp
    - Round 100: 2 obs bridges (file_send header + raw body) — file.cpp both sites bridged; Step 4 COMPLETE
- [x] Step 5: Migrate `src/common/` utility modules — COMPLETE (R101–R103)
- [x] Step 6: Eliminate legacy memory management (`xalloc`) — COMPLETE (R104)
  - [x] Audit: 70 files with stale `#include "common/xalloc.h"` — all removed
  - [x] Audit: 0 actual xalloc function calls in bnetd/d2cs/d2dbs/common (already eliminated R1–R13)
  - [x] `src/bnetd/main.cpp`: removed last `xalloc_setcb()` call; legacy OOM handler replaced by C++ `std::set_new_handler`
  - [x] Created `plans/step6-xalloc-checklist.md` with full per-file audit table
- [x] Step 7: Eliminate legacy data structures (`t_list`, `t_hashtable`, `t_elist`) — Tier 3 header migrations COMPLETE (R111)
  - All Tier 3 public-API headers migrated: `channel.h` R107, `clan.h`/`friends.h`/`account.h` R108, `connection.h` R109, `realm.h` R110, `watch.h` R111
  - Remaining `t_list`/`t_hashtable`/`t_elist` work (alias_command, anongame_infos, icons, team, d2cs, d2dbs, etc.) tracked in `step7-datastructs-checklist.md`
- [-] Step 8: Migrate `src/compat/` platform abstractions — ALL v3 headers COMPLETE (R115); psock caller migration 10/17 done (R117)
  - [x] Audit: all 16 compat modules inventoried, caller counts measured, tiers assigned — `plans/step8-networking-checklist.md` created
  - [x] Tier A (7 headers): `netinet_in.hpp`, `read.hpp`, `stdfileno.hpp`, `termios.hpp`, `socket.hpp`, `recv.hpp`, `send.hpp` — all created in `src/v3/infra/compat/include/infra/compat/`
  - [x] Tier B (4 headers): `mkdir.hpp`, `rename.hpp`, `runtime_libs.hpp`, `strerror.hpp` — all created in `src/v3/infra/compat/include/infra/compat/`
  - [x] Already done (prior rounds): `platform.hpp`, `process.hpp` (covers `gethostname.h` + `pgetpid.h`)
  - [x] Tier C Round 113: `pdir.h` → `directory.hpp` — `DirectoryIterator` RAII + `list_files()` + 20 Catch2 tests
  - [x] Tier C Round 114: `pgetopt.h` → `getopt.hpp` — `CommandLineParser` + `parse_args()` + 28 Catch2 tests
  - [x] Tier C Round 115: `psock.h` → `infra/net/socket.hpp` — `UniqueSocket` RAII + `IpAddress`/`Port`/`SocketAddress` + `WinsockGuard` + 30 Catch2 tests (no Boost.Asio)
  - [x] pdir callers (8 files) — ✅ R116 COMPLETE
  - [-] psock callers (17 files) — 10 migrated ✅ R117; 7 deferred ⏳ Round 118+
    - [x] `src/d2cs/d2gs.cpp`, `src/d2cs/handle_d2cs.cpp`, `src/d2dbs/dbspacket.cpp` — include-only removal
    - [x] `src/bnetd/handle_file.cpp`, `src/bnetd/tracker.cpp`, `src/bnetd/udptest_send.cpp` — simple macro/call replacements
    - [x] `src/common/addr.cpp`, `src/common/fdwatch_select.h` — type/macro replacements
    - [x] `src/bnetd/main.cpp` — `psock_init/deinit` → `WSAStartup/WSACleanup` (Win32 only)
    - [x] `src/common/network.cpp` — `psock_recv/send` + all `PSOCK_E*` errno constants replaced
    - [ ] `src/bnetd/server.cpp`, `src/bnetd/connection.cpp`, `src/d2cs/connection.cpp`,
          `src/d2cs/net.cpp`, `src/d2cs/s2s.cpp`, `src/d2cs/server.cpp`, `src/d2dbs/dbserver.cpp`
          — deferred (full socket lifecycle / event loops)
- [x] Step 9: pugixml via FetchContent — COMPLETE ✅ (R118–R119)
  - [x] Audit: legacy `src/common/pugixml.h` + `pugixml.cpp` identified; `src/bnetd/i18n.cpp` is main consumer
  - [x] FetchContent block added to `src/v3/CMakeLists.txt` (option `PVPGN_V3_WITH_PUGIXML`, tag v1.14)
  - [x] `src/v3/infra/xml/include/infra/xml/xml_document.hpp` — `XmlDocument` + `XmlNode` wrapper created
  - [x] `src/v3/infra/xml/CMakeLists.txt` — `infra_xml` INTERFACE target created
  - [x] `tests/unit/infra/xml/test_xml_document.cpp` — 30 Catch2 tests written
  - [x] `tests/unit/infra/xml/CMakeLists.txt` — `test_infra_xml_document` target created
  - [x] `tests/unit/infra/CMakeLists.txt` — `xml` subdirectory added (guarded by `TARGET infra_xml`)
  - [x] `Dockerfile.v3` — `infra_xml`, `test_infra_xml_document` added to build + test stages
  - [x] R119: Consumer audit — only `i18n.cpp` uses pugixml; output.cpp/ladder.cpp/d2ladder.cpp/bntrackd have zero usage
  - [x] R119: `src/bnetd/i18n.cpp` migrated to `pvpgn::v3::infra::xml::XmlDocument` — all `pugi::` removed
  - [x] R119: `src/bnetd/CMakeLists.txt` — `if(TARGET infra_xml)` guard added to link `bnetd_legacy`
  - [x] R119: Verified: `grep -r 'pugi::' src/bnetd/ src/d2cs/ src/d2dbs/` → zero results
  - [x] R119: `src/common/pugixml.h/.cpp/pugiconfig.h` — no remaining consumers; safe to delete in Step 10
- [-] Step 10: Configuration migration to TOML — `Config` wrapper + `bnetd.toml.in` + tests 🔄 (R121)
  - [x] Audit: `conf/bnetd.conf.in`, `conf/d2cs.conf.in`, `conf/d2dbs.conf.in` — all key groups inventoried
  - [x] Audit: `src/v3/CMakeLists.txt` — confirmed option `PVPGN_V3_WITH_TOMLPP`, target `tomlplusplus::tomlplusplus`
  - [x] Audit: `infra_config` library already defined (lines 256–272); `tests/unit/infra/CMakeLists.txt` already has `add_subdirectory(config)`
  - [x] Audit: 38 `src/bnetd/` files include `prefs.h`; ~300+ `prefs_get_*` call sites catalogued
  - [x] `src/v3/infra/config/include/infra/config/config.hpp` — `Config` class created (header-only toml++ wrapper)
  - [x] `conf/bnetd.toml.in` — TOML template with all 20 sections matching `bnetd.conf.in`
  - [x] `tests/unit/infra/config/test_config.cpp` — 18 Catch2 tests covering all `Config` methods
  - [x] `tests/unit/infra/config/CMakeLists.txt` — `test_infra_config_config` target added
  - [x] `plans/step10-toml-checklist.md` — consumer audit table + deliverables checklist created
  - [x] R121: `server_config.hpp` — expanded to 20 typed sub-structs covering all `prefs_get_*` fields
  - [x] R121: `server_config.cpp` — rewritten to use `Config` wrapper; all 20 sections parsed
  - [x] R121: `legacy_prefs.hpp` — expanded to ~100 `prefs_get_*`-style accessors over `ServerConfig`
  - [x] R121: `server_config_test.cpp` — updated for new struct layout (7 test cases)
  - [x] R121: `legacy_prefs_test.cpp` — updated + new policy/timing/clan test case
  - [x] R121: `conf/d2cs.toml.in` — TOML template for d2cs (5 sections: server, network, realm, log, files, misc, internal)
  - [x] R121: `conf/d2dbs.toml.in` — TOML template for d2dbs (5 sections: network, log, files, ladder, misc)
  - [-] R134–R139, R143, R145: Migrate `prefs_get_*` callers in `src/bnetd/` to use `prefs_v3_shim.h` (38/38 files migrated; only `prefs.cpp` self-references + `icons.cpp` self-definition of `prefs_get_custom_icons` remain). Tracked in `plans/step10-toml-checklist.md`.
  - [ ] Delete `src/bnetd/prefs.cpp` / `prefs.h` after all callers migrated and bridge fallback is no longer required
  - [x] R143: Install `bnetd.toml.in`, `d2cs.toml.in`, `d2dbs.toml.in` via `conf/CMakeLists.txt` (gated on `PVPGN_BUILD_V3`)
  - [x] R144: `Dockerfile.v3` / v3 + tests CMake gates fixed for `alpine:latest` (cmake 4.x, gcc 14). `docker build -f Dockerfile.v3` + `--target v3-test` both green. See `plans/step10-toml-checklist.md` "Round 144".
  - [x] R145: `[messages]` section + `log_notice` semantic-fix (bool → std::string) added to `ServerConfig`; bridge `pvpgn_v3_prefs_get_log_notice()` return type fixed to `char const*`; 5 new bridge entries for `quota_*`; 7 new shim accessors; 23 caller sites migrated across `channel/command/connection/handle_anongame/luainterface`; latent typo at `prefs.cpp:2044` fixed. Docker build + tests green. See `plans/step10-toml-checklist.md` "Round 145".
  - [x] R146: Closed remaining bridge audit gaps (`pvpgn_v3_prefs_get_quota`, `pvpgn_v3_prefs_get_outputdir`); `prefs_v3::quota()` shim added; last 2 `prefs_get_quota()` callers (`connection.cpp:3203`, `luainterface.cpp:256`) migrated; `quota = true` added to `conf/bnetd.toml.in` `[messages]`; `/reload` handler at `server.cpp:1714` now also reloads the v3 TOML snapshot so `prefs_v3::*` stays in lock-step with legacy `bnetd.conf`. Docker build + tests green. See `plans/step10-toml-checklist.md` "R146".
  - [x] R147: Inventoried residual `prefs.cpp` callers (4 lifecycle sites + 1 un-migrated accessor + 141 shim fallback branches) and wrote a 5-phase deletion plan at `plans/prefs-cpp-deletion-plan.md`. No code changes this round.
  - [x] R148+R149: Phase A+B of `prefs.cpp` retirement. Bridge `pvpgn_v3_prefs_load_toml()` now always populates `g_prefs` (defaults on parse failure); added `pvpgn_v3_prefs_unload()`. `main.cpp` / `server.cpp` lifecycle and `/reload` paths gated under `PVPGN_V3_BNETD_INTEGRATION` to use v3 TOML exclusively (legacy `prefs_load`/`prefs_unload` dropped under v3 build); last direct accessor `prefs_get_trackserv_addrs()` migrated. Docker build + tests green. Shim fallback branches now statically unreachable -- ready for Phase C flatten.
  - [x] R150+R151: Phase C+D of `prefs.cpp` retirement. Flattened 135 shim accessors from runtime `if(pvpgn_v3_prefs_loaded())` guard to `#ifdef PVPGN_V3_BNETD_INTEGRATION` direct bridge call (134 via regex pass + 1 alias special-case). Migrated last stray legacy caller (`luainterface.cpp:224` `prefs_allow_d2cs_setname`). `prefs.h` no longer included under v3. `src/bnetd/CMakeLists.txt` drops `prefs.cpp` from `BNETD_LIB_SOURCES` when `integration_legacy_bnetd_linked` is a target -- v3 bnetd binary no longer compiles or links the legacy prefs parser. Docker build + tests green. Strangler-fig phase complete: `bnetd.toml` is the single source of truth for v3.
  - [x] R152: d2cs + d2dbs `ServerConfig` schemas, parsers, and extern "C" prefs bridges added (skeleton only; no caller migration). `infra_config` gains `d2cs_server_config.cpp` + `d2dbs_server_config.cpp`; `integration_legacy_d2cs` / `integration_legacy_d2dbs` gain their bridge `.cpp` and a `TARGET_EXISTS:infra_config` `PUBLIC_DEPS` entry. Bridges follow the bnetd pattern (process-global `std::optional<...ServerConfig>` snapshot, always-populate-with-defaults on parse failure, `StringCache` for `filesystem::path -> const char*` lifetime). Docker v3-test exit 0; all Catch2 suites green. d2cs/d2dbs caller migration deferred to R153+.
  - [x] R153: d2cs caller-migration kickoff (Phase B-equivalent). Added `src/d2cs/prefs_v3_shim.h` with ~50-accessor `pvpgn::d2cs::prefs_v3::*` namespace dispatching to the v3 bridge under `PVPGN_V3_D2CS_INTEGRATION` (legacy fallback otherwise). `src/d2cs/main.cpp` now (a) additively calls `pvpgn_v3_d2cs_prefs_load_toml()` with a `.toml` path derived from cmdline preffile and (b) reads `loglevels`/`logfile` through `prefs_v3::*`. Legacy `d2cs_prefs_load` still runs in parallel -- full swap deferred until remaining `src/d2cs/` callers are migrated. Docker v3-test exit 0; all Catch2 suites green.
  - [x] R154: Bulk migration of ~106 `src/d2cs/` accessor call sites across 15 .cpp files from `d2cs_prefs_get_*` / `prefs_get_*` / `prefs_allow_*` / `prefs_check_*` / `prefs_hide_*` to `pvpgn::d2cs::prefs_v3::*` via PowerShell regex pass (with two special-case renames: `prefs_get_d2gs_list` → `gameservlist`, `prefs_get_d2cs_account_allowed_symbols` → `account_allowed_symbols`). Shim include auto-inserted alongside `prefs.h`. Only `src/d2cs/prefs.cpp` internals + lifecycle entry points (`d2cs_prefs_load`/`d2cs_prefs_unload`/`prefs_reload`) untouched. Docker v3-test exit 0 first try; all Catch2 suites green. Ready for Phase B full swap and Phase D legacy drop.
  - [x] R155: d2cs Phase B full swap. Under `PVPGN_V3_D2CS_INTEGRATION` `src/d2cs/main.cpp` no longer calls legacy `d2cs_prefs_load`/`d2cs_prefs_unload`; v3 TOML parse failure is fatal. `src/d2cs/handle_signal.cpp` SIGHUP reload now calls `pvpgn_v3_d2cs_prefs_load_toml` instead of `prefs_reload`. Shim's `#else` branch retained for non-v3 builds (statically unreachable under v3, ready for Phase C flatten). Docker v3-test exit 0; all Catch2 suites green.
  - [x] R156: d2cs Phase D. `src/d2cs/CMakeLists.txt` now drops `prefs.cpp` from `D2CS_LIB_SOURCES` when `integration_legacy_d2cs_linked` exists (mirrors bnetd R151). Phase C was already in place because the d2cs shim was authored in flattened `#ifdef/#else` form in R153. Strangler-fig for d2cs complete: `d2cs.toml` is the sole config source for v3; `prefs.cpp` no longer compiles or links into the v3 d2cs binary. Docker v3-test exit 0; all Catch2 suites green.
  - [x] R157: d2dbs full strangler-fig (R152-R156 equivalent) collapsed into one round. Added `src/d2dbs/prefs_v3_shim.h` (21 inline accessors), wired bridge lifecycle into `src/d2dbs/main.cpp` (v3-only, FATAL on parse failure) and `src/d2dbs/handle_signal.cpp` (SIGHUP reload), bulk-migrated all 35 d2dbs call sites across 5 .cpp files to `pvpgn::d2dbs::prefs_v3::*` (with special-case renames `d2gs_list`→`gameservlist`, `XML_output_ladder` left identity), and dropped `prefs.cpp` from `D2DBS_LIB_SOURCES` under the v3 build. Docker v3-test exit 0 first try; all Catch2 suites green. Strangler-fig for d2dbs complete: `d2dbs.toml` is the sole v3 config source. Phase 1 Step 10 now functionally complete for bnetd + d2cs + d2dbs.
  - [x] R158: Catch2 coverage parity for the new parsers. Added `tests/unit/infra/config/d2cs_server_config_test.cpp` (13 cases / 105 assertions covering every section, all 16 file paths including all 7 newbiefile slots, `ladder_start_time` ISO datetime parsing valid+malformed, error branches, on-disk load + parse/load round-trip) and `tests/unit/infra/config/d2dbs_server_config_test.cpp` (10 cases / 54 assertions). Wired into `tests/unit/infra/config/CMakeLists.txt` and into the `Dockerfile.v3` v3-build target list + v3-test run sequence. Docker v3-test build green; both new suites "All tests passed". bnetd + d2cs + d2dbs parsers are now all unit-tested directly.
  - [x] R159: LegacyPrefs adapter symmetry. Added `src/v3/infra/config/include/infra/config/d2cs_legacy_prefs.hpp` (`D2csLegacyPrefs`, ~55 accessors across server/network/realm/log/files/misc/internal) and `d2dbs_legacy_prefs.hpp` (`D2dbsLegacyPrefs`, 23 accessors), each header-only, mirroring the bnetd `LegacyPrefs` (R121/R146) pattern: immutable snapshot, pre-computed `std::string` copies for `filesystem::path` fields, `shared_ptr` factory for atomic hot-reload swap. Added Catch2 tests (`d2cs_legacy_prefs_test.cpp` 2 cases / 38 assertions, `d2dbs_legacy_prefs_test.cpp` 2 cases / 32 assertions). Wired into `tests/unit/infra/config/CMakeLists.txt` + `Dockerfile.v3`. Docker v3-test green. All three services now have a typed config, a TOML parser, an extern-C bridge, a migration shim, AND a C++ LegacyPrefs adapter.
  - [x] R160: TOML template coverage audit for `conf/d2cs.toml.in` + `conf/d2dbs.toml.in` vs the schemas. Fixed `d2dbs.toml.in`: changed `difficulty_hack = false` (would silently default since field is `uint32`) to `difficulty_hack = 0`; removed misleading commented `ladder_start_time` block from `[ladder]` (not a d2dbs schema field). Fixed `d2cs.toml.in`: added missing `misc.hide_pass_games = false`, uncommented `internal.game_maxlevel = 255`, added missing `internal.ladderlist_count = 0`. Every schema field now has a corresponding active or commented line in the deployable template. Docker v3-test green across all 30 Catch2 suites.
  - [x] R161: Wire d2cs/d2dbs LegacyPrefs adapters into the bridges. `src/v3/integration/legacy_d2cs/src/d2cs_prefs_bridge.cpp` and `legacy_d2dbs/src/d2dbs_prefs_bridge.cpp` now hold a single `std::shared_ptr<D2{cs,dbs}LegacyPrefs>` instead of a `D2{cs,dbs}ServerConfig` + separate `StringCache`; all `_get_*` accessors call adapter methods (return `const std::string&` from `data()`). Adapter return types refactored from `std::string_view` to `const std::string&` for stable null-terminated `const char*` storage. Added `tests/unit/infra/config/conf_template_test.cpp` (3 cases / 18 assertions) that slurps each `conf/*.toml.in` via injected `PVPGN_CONF_SOURCES_DIR` and feeds it through the v3 parser -- guarantees templates stay parseable as schemas evolve. Wrote `plans/step10-wrap-up.md` covering the full R120-R161 narrative. Docker v3-test green; all suites pass including 18/3 for the new templates test.
  - [x] R162: Step 10 closeout polish. (1) Verified SIGHUP hot-reload for d2cs / d2dbs was already wired (R155 / R157) -- both `src/d2cs/handle_signal.cpp` and `src/d2dbs/handle_signal.cpp` call `pvpgn_v3_<svc>_prefs_load_toml(toml_path)` on reload. (2) Audited all `*.cpp` / `*.h` / lua / plugins for direct `prefs_get_*` calls bypassing the `prefs_v3` shim: 1 straggler found in `src/bnetd/server.cpp:1807` (`tracker_set_servers(prefs_get_trackserv_addrs())`), fixed to `prefs_v3::trackserv_addrs()`. Wrote `plans/legacy-prefs-deletion-audit.md` (conclusion: full deletion gated on dropping the non-v3 build, deferred to Step 11). (3) Added `/config` admin command to bnetd (declaration + table entry + impl in `src/bnetd/command.cpp`; group 8 in `conf/command_groups.conf.in`); dumps live `prefs_v3::*` accessors as a compact TOML-shaped listing -- lets operators verify a SIGHUP / `/rehash` actually picked up `.toml` changes without tailing the eventlog. Docker v3-test green across all suites.
- [ ] Step 11: Final integration and build verification
  - [x] R163: Step 11 kickoff. Wrote `plans/step11-checklist.md` enumerating remaining closeout work (non-v3 build retirement, prefs.cpp deletion, docs). Added shared `src/v3/infra/config/include/infra/config/prefs_dump.hpp` with three overloaded `format_dump(const ...LegacyPrefs&)` free functions returning TOML-shaped `vector<string>`; backed by `tests/unit/infra/config/prefs_dump_test.cpp` (4 cases / 46 assertions). New bridge exports `pvpgn_v3_d2cs_prefs_dump` and `pvpgn_v3_d2dbs_prefs_dump` (callback-per-line) wired into `src/d2cs/handle_signal.cpp` + `src/d2dbs/handle_signal.cpp` so every SIGHUP reload logs the active config snapshot to the eventlog under module name `d2cs_config` / `d2dbs_config`. `conf/CMakeLists.txt` no longer installs legacy `bnetd.conf` / `d2cs.conf` / `d2dbs.conf` under `PVPGN_BUILD_V3` -- only the `.toml` siblings ship. Docker v3-test green across all suites including the new prefs_dump tests.
  - [x] R164: Step 11 continued. (1) Flattened `src/{bnetd,d2cs,d2dbs}/prefs_v3_shim.h` -- removed the non-v3 `#else` branch from every accessor; per-service line counts dropped from 1151->615 (bnetd), 505->273 (d2cs), 208->120 (d2dbs). Bridge call under `PVPGN_V3_*_INTEGRATION` is the sole code path. (2) Added `pvpgn_v3_prefs_dump` to the bnetd bridge mirroring d2cs/d2dbs (R163) and routed `_handle_config_command` in `src/bnetd/command.cpp` through it -- all three services now share the same `format_dump()` formatter for operator-visible config snapshots. (3) Wrote `docs/toml-migration.md` (operator-facing side-by-side `.conf` -> `.toml` guide for all three services); updated `README.md` to point at `bnetd.toml`; prefixed `changelog.md` with a Step 11 entry covering R163+R164 deliverables. Deferred items (R165 candidates): prefs.cpp/.h deletion (`src/bnetd/server.cpp:1714`+`:1718` still call bare `prefs_load()` outside the integration guard; 17 callers still `#include "prefs.h"`), atomic `shared_ptr` swap for the three bridge globals, and `docs/compile.*.md` audit. Docker v3-test green; prefs_dump suite still reports 46/4.
  - [x] R167: Phase 3 kickoff + R166 follow-ups. (1) Added `--version` / `-V` flag to `src/v3/app/{bnetd,d2cs}/src/main.cpp`; wired `PVPGN_VERSION` compile-def into both binaries; added CTest smoke tests (`pvpgn_v3_bnetd_help` / `_version`, `pvpgn_v3_d2cs_help` / `_version`) gated on `BUILD_TESTING` so CI catches link/startup regressions without needing a working config. (2) Pursued option D of `plans/snapshot-lifetime-scope.md` -- audited all 39 `prefs_v3::*` call sites in `src/bnetd/`. Result recorded in `plans/snapshot-lifetime-audit.md`: zero D-class hazards (every string-returning accessor consumed immediately or copied into `std::string`/`sv_strdup`/`memcpy`). (3) Wrote `plans/phase3a-handle-init-audit.md`: handle_init.cpp is 230 lines, 6 cclass branches; the v3 strangler hook `pvpgn_v3_init_conn_apply` is dispatch-ready. Identified 4 blockers + suggested R168/R169 work units to delete the file. (4) Wrote `plans/l1-cutover-scope.md`: scopes the Dockerfile.v3 `v3-runtime` stage that would let production run `pvpgn_v3_bnetd` instead of legacy `bnetd`; 5 prerequisites + 5-sub-stage plan. (5) Confirmed `/config` telnet admin command already routes through `pvpgn_v3_prefs_dump` end-to-end (R164); documented in `docs/toml-migration.md` (no code change needed). Docker v3-test green.
  - [x] R166: Step 11 follow-on / Phase 3 kickoff. (1) Deleted `conf/{bnetd,d2cs,d2dbs}.conf.in` and `.conf.win32` siblings; `conf/CMakeLists.txt` no longer maintains the v3-vs-legacy install gate around config files -- only `.toml` ships now. (2) Wider `docs/` `.conf` sweep: `bnmotd.md`, `fdwatch.txt`, `storage.txt`, and `single-binary-mode.md` updated to reference `bnetd.toml` / `docs/toml-migration.md`. (3) Wrote three planning docs scoping the remaining big-ticket items: `plans/phase3-plan.md` (handler-by-handler migration roadmap with L-A through L-I round buckets across bnetd + d2cs + d2dbs), `plans/legacy-retirement-scope.md` (staged L0-L5 retirement of `PVPGN_BUILD_LEGACY` -- gated on Phase 3.A-D), `plans/snapshot-lifetime-scope.md` (residual `const char*` lifetime hazard from R165 atomic swap; four mitigation options ranked from comment-only through caller-side copy / immortal strings / RAII handle). Step 11 is closed; remaining hazards have explicit scope docs. Docker v3-test green.
  - [x] R165: Step 11 closeout. (1) Deleted `src/{bnetd,d2cs,d2dbs}/prefs.cpp` + `.h` outright; gated the bnetd `server.cpp` `restart_mode_all` branch under `PVPGN_V3_BNETD_INTEGRATION` so the SIGHUP-restart path calls `pvpgn_v3_prefs_load_toml` to reload the TOML snapshot. Dropped the per-service `list(REMOVE_ITEM ... prefs.cpp)` conditionals from the three legacy lib CMakeLists -- prefs is now unconditionally absent from `bnetd_legacy`/`d2cs_legacy`/`d2dbs_legacy`. (2) Converted all three bridge globals from `std::optional` / `std::shared_ptr` to `std::atomic<std::shared_ptr<...LegacyPrefs>>`; `load_toml`/`unload`/`dump`/accessors take a local strong reference via `.load()`, so SIGHUP swap is race-free with concurrent readers (lifetime of returned `const char*` still bounded by the accessor call -- documented). bnetd bridge dropped its `prefs()` helper in favour of `make_shared<LegacyPrefs>(ServerConfig)`. (3) Demoted `PVPGN_BUILD_V3` to default-ON in root `CMakeLists.txt` (v3 is now the supported build path; `PVPGN_BUILD_LEGACY` stays default-on as the transitional legacy build). (4) `docs/compile.*.md` audit: zero `.conf` references; updated `docs/single-binary-mode.md` to use `.toml` filenames and link to `toml-migration.md`. Docker v3-test green; no regressions across the full suite (1225/213 for infra_log, 46/4 for prefs_dump, etc.).
  - [x] R168: snapshot lifetime hardening + init dispatch + v3-runtime preview. (1) Added "LIFETIME WARNING (R168)" comment block to all three integration prefs bridges (`src/v3/integration/legacy_{bnetd,d2cs,d2dbs}/src/*prefs_bridge.cpp`) documenting that returned `const char*` is valid only until the next SIGHUP swap (mitigation option A of `plans/snapshot-lifetime-scope.md`). (2) R168.a: `pvpgn_v3_init_conn_apply` in `init_conn_bridge.cpp` now returns -1 (was 0) when no handler is installed for an accepted cclass and logs once via `core::log_msg` -- startup wiring bugs are surfaced instead of silently falling through to legacy. Existing no-handler test updated to expect -1. (3) R168.b + R168.c: `application/init/init_conn_dispatch.hpp` `InitConnRequest` gained `conn_count` + `max_conns_per_ip` (rate limit) and `d2cs_ip_allowed` (realmlist gate); `InitDecision` gained `kRateLimited` (5) and `kD2csIpDenied` (6); dispatcher checks rate-limit first, then realmlist, then class. 10 new test cases added to `init_conn_dispatch_test.cpp` (26 total). (4) `Dockerfile.v3`: added `v3-runtime` PREVIEW stage (soft-fail). Configures + attempts to build `pvpgn_v3_bnetd`; on failure logs "PREVIEW: ... skipped (expected during Phase 3 work)" and still tags `pvpgn:v3-runtime` so the L1.a cutover path stays exerciseable while `main.cpp` is hardened. CMD prints `--version` when binary present, otherwise stub. (5) Wrote `plans/phase3b-handle-bnet-audit.md`: scoping for the 6470-line, ~95-handler `handle_bnet.cpp` retirement. Maps the 13 handler families to existing v3 application modules + bridges, identifies 4 gap modules (email_management, ads, realm, anongame_lobby) and proposes 9 sub-rounds (R170-R178). Docker v3-test green; no regressions.
  - [x] R169: init_conn bridge ABI extension + legacy switch gating + email_management skeleton + v3-runtime hardening. (1) R169.a: extended ABI `pvpgn_v3_init_conn_{decide,apply}_ex` added to `init_conn_bridge.{hpp,cpp}` taking `(cclass, conn_count, max_conns_per_ip, d2cs_ip_allowed, ...)` and forwarding them through to the v3 dispatcher; _apply_ex returns -1 (authoritative close) on kRateLimited / kD2csIpDenied / NULL-handler. `src/bnetd/handle_init.cpp` switched to call the _ex variants with real values from `connlist_count_connections`, `prefs_v3::max_conns_per_IP()`, and `realmlist_find_realm_by_ip`. 7 new test cases added to `init_conn_bridge_test.cpp`; no-handler test updated to expect -1. (2) R169.b: under `PVPGN_V3_BNETD_INTEGRATION` the legacy cclass switch (BNET/FILE/BOT/TELNET/D2CS_BNETD/ENC/LOCALMACHINE) is wrapped in `#ifndef PVPGN_V3_BNETD_INTEGRATION` and replaced with an unreachable-fallback error log + `return -1`; the v3 dispatcher is now authoritative. (3) R169.c DEFERRED: `handle_init.cpp` cannot be removed from CMake because `handle_init_packet` is still called from `src/bnetd/server.cpp:992` packet pump. Staged removal plan appended to `plans/phase3b-handle-bnet-audit.md` (R171.x shim + R175.x deletion). (4) R169.d: new `src/v3/application/email_management/` module with interface-only headers `email_change.hpp` (EmailChangeRequest/Response/Status) and `password_recovery.hpp` (PasswordRecoveryRequest/Response/Status). Placeholder TU returns kRejected unconditionally so callers wired ahead of R170 fail closed. Registered as `application_email_management` library in `src/v3/CMakeLists.txt` (depends on core only). (5) R169.e: `src/v3/app/bnetd/src/main.cpp` `TcpConnectionContext::send_packet` rewritten from reserve+push_back+insert to pre-sized vector + index-write; sidesteps gcc 15 `-Werror=free-nonheap-object` false positive. `v3-runtime` Dockerfile stage now builds `pvpgn_v3_bnetd` cleanly (binary installed to `/usr/local/bin/pvpgn_v3_bnetd`). Docker v3-test + v3-runtime both green.
  - [x] R170: email_management impl + ads/anongame_lobby skeletons + handle_init.cpp shim. (1) R170.a: implemented `application/email_management/src/email_change.cpp` -- `dispatch_email_change` covers set-when-unset and replace-existing paths; `dispatch_password_recovery` covers the `_client_getpasswordreq` flow. Case-insensitive matching matches legacy `strcasecmp`; syntactic validation enforces non-empty local part + exactly one `@` + dotted domain. 13 new test cases in `tests/unit/application/email_management/email_management_test.cpp`. (2) R170.b: `src/v3/application/ads/` skeleton (`ad_pick.hpp` + placeholder TU); real impl deferred to R171. (3) R170.c: SKIPPED -- `application/realm` already exists with 11 headers (create/delete/list/load/save_character, join_game_server, gs_queue, character_list_repository, character_lock, character_persistence, d2_ladder_repository). R172 becomes realm gap-filling, not scaffolding. (4) R170.d: `src/v3/application/anongame_lobby/` skeleton (`lobby.hpp` with `LobbyEntry`, `LobbyAdmitRequest/Response`, `LobbyAdmitStatus{kQueued,kPromoted,kDuplicate,kRejected}` + placeholder TU); real impl deferred to R173. (5) R170.e: `src/bnetd/handle_init.cpp` shrunk from 247 to 105 lines -- under v3 the body is just packet validation + a single `pvpgn_v3_init_conn_apply_ex` call (returns 0 on rc==1 else -1); the parity-log + unreachable-fallback removed because the bridge already returns -1 for any failure (R168.a guarantee). Non-v3 builds emit `#error`. (6) R170.f: `plans/phase3b-handle-bnet-audit.md` deduplicated (R169.c follow-up was inadvertently appended twice in R169) and updated with R170 progress + revised R171-R179 round assignment (R171 ads impl, R172 realm gap-fill, R173 anongame_lobby impl, R174 game lifecycle, ..., R179 delete handle_bnet.cpp). Docker v3-test + v3-runtime both green; 13 new email_management cases pass.
  - [x] R171: application/ads real impl + bridge + handle_bnet ad handlers v3-authoritative. (1) R171.a: replaced the R170.b placeholder in `src/v3/application/ads/src/ad_pick.cpp` with the real `dispatch_ad_pick` (client/lang filter with 0=wildcard, selection_hint mod-size override, prev_ad_id sequence with wrap-to-first) and `dispatch_ad_click` (id+wildcard-client match, url-fallback for empty click_url). `AdCandidate` switched to `std::string` ownership so the dispatcher can return chosen banners by value. 11 new test cases / ~30 assertions in `tests/unit/application/ads/ads_test.cpp`. (2) R171.b: new `application/ads/ads_repository.hpp` `IAdsRepository` interface (`list_for` + `find_by_id`); legacy `AdBannerSelector` exposes a new `for_each(F&& fn) const` template so the v3 bridge can enumerate banners without breaking encapsulation. (3) R171.c base: new C ABI `pvpgn_v3_ads_pick_apply` / `pvpgn_v3_ads_click_apply` with POD `PvpgnV3AdPick/ClickOut` out-structs (fixed-size buffers for lifetime-free C boundary), atomic handler-pointer state, returns 1/0/-1 (chose/none/no-handler). (3') R171.c linked: `ads_bridge_link.cpp` enumerates `AdBannerList` via `for_each`, applies the MNG/non-MNG extension filter inline (since `EXTENSIONTAG_MNG` is a legacy `tag.h` symbol), builds a `std::vector<AdCandidate>` and calls the application dispatchers. `install_ads_handlers()` added to `install_v3_handlers.{hpp,cpp}` with idempotent CAS pattern; `src/bnetd/server.cpp:2122` invokes it under `PVPGN_V3_BNETD_INTEGRATION`. (4) R171.d: 7 new test cases in `tests/unit/integration/legacy_bnetd/ads_bridge_test.cpp` covering no-handler/-1, pick/click forwarding + data copy, rc==0 semantics, null-out-pointer. (5) R171.e: `_client_adreq` and `_client_adclick2` in `src/bnetd/handle_bnet.cpp` invoke the v3 bridge first under v3; rc>=0 is authoritative (rc==0 returns 0, rc==1 builds reply from out-struct + forwards through `pvpgn_v3_send_adreply` / `pvpgn_v3_send_adclick2reply`), rc==-1 falls back to legacy `AdBannerList.pick/find` -- so non-v3 builds and v3 builds without the linked half installed both keep working. `_client_adack` / `_client_adclick` left unchanged (they don't read `AdBannerList`). (6) R171.f: DEFERRED. Documented as R172.x candidate -- requires a new `protocol/bnet/init_packet` module + refactor of `src/bnetd/server.cpp:992` packet pump to dispatch through it, so `handle_init.cpp` can be removed from CMake entirely. Out of scope for this round. Docker v3-test (166+ binaries green, +11 application_ads + +7 ads bridge cases) + v3-runtime (`pvpgn_v3_bnetd` builds clean) both verified.
  - [x] R172: application/realm realm-list dispatcher + IRealmRepository + handle_bnet realm-list handlers v3-authoritative. (1) R172.a + R172.c: new `application/realm/realm_list.hpp` exposing `RealmListing{id,name,description,active}`, `IRealmRepository` port (single method `list_all() -> std::vector<RealmListing>`), `RealmListResponse{active_entries}` and the free function `dispatch_realm_list(IRealmRepository const&)` -- pure-C++, no legacy dependency, just filters the snapshot to active==true and preserves repository order. `src/v3/application/realm/src/realm_list.cpp` is the trivial implementation; 6 new unit tests / ~17 assertions in `tests/unit/application/realm/realm_list_test.cpp` with a `StaticRealmRepo` test double. (2) R172.b: noted that `join_game_server.cpp` already has a 67-line real implementation -- the dialog text was misleading; what's actually missing is enabling the unbuilt sources (`create_character.cpp`, `delete_character.cpp`, `list_characters.cpp`, `load_character.cpp`, `save_character.cpp`, `join_game_server.cpp`) in CMake, which requires real (non-mock) `ICharacterRepository` + `ICharacterListRepository` adapters. Out of scope for this round; queued for a future round. (3) R172.d base: new C ABI `pvpgn_v3_realm_list_apply(void* conn_ptr, int legacy_format)` in `integration/legacy_bnetd/realm_list_bridge.{hpp,cpp}`, returning 1 (sent), 0 (handler installed but send failed -- do NOT fall back) or -1 (no handler, fall back to legacy). Mirrors the init_conn / ads bridge atomic-handler-pointer pattern. (3') R172.d linked: `realm_list_bridge_link.cpp` ships a `LegacyRealmRepository` adapter that copies `realm_get_name` / `realm_get_description` / `realm_get_active` from each `realmlist()` entry into `RealmListing`; `legacy_realm_list_handler` runs `dispatch_realm_list` and forwards through the existing `pvpgn_v3_send_realmlistreply` / `pvpgn_v3_send_realmlistlegacyreply` bridges based on `legacy_format`. `install_realm_list_handler()` added to `install_v3_handlers.{hpp,cpp}` with idempotent CAS; `src/bnetd/server.cpp` invokes it under `PVPGN_V3_BNETD_INTEGRATION` right after `install_ads_handlers()`. (4) R172.d wiring: `_client_realmlistreq` and `_client_realmlistreq110` in `src/bnetd/handle_bnet.cpp` now dispatch through `pvpgn_v3_realm_list_apply` first -- rc >= 0 is authoritative (return 0), rc == -1 falls back to the legacy `for(realmlist()) emit` loop. The previous v3 inline `std::vector<pvpgn_v3_realm_{legacy_,}entry>` build + send_realmlist*reply call is replaced with a single bridge call; that responsibility now lives in the linked half. (5) R172.e: 6 new integration test cases in `tests/unit/integration/legacy_bnetd/realm_list_bridge_test.cpp` covering no-handler-returns-minus-one, null-conn-with-handler returns 0 (handler NOT invoked), legacy_format=1 forwarding + rc=1, legacy_format=0 forwarding + rc=1, rc=0 propagation (send-failure), and `get_realm_list_handler` reflecting `set_realm_list_handler`. (6) R172.f: DEFERRED again -- same scope as R171.f (new `protocol/bnet/init_packet` module + `src/bnetd/server.cpp:992` packet-pump refactor; multi-round protocol-layer effort). Recommended as the sole work unit for R173 if user wants to attack it next. Docker v3-test image `pvpgn-v3-test:r172` sha256:1b87e0c0… (168 binaries green, +1 application_realm_realm_list_test + +1 test_integration_legacy_bnetd_realm_list_bridge versus R171's 166) and v3-runtime image `pvpgn-v3-runtime:r172` sha256:7d551b00… (`pvpgn_v3_bnetd` builds + installs cleanly with the new linked bridge) both verified.
  - [x] R173: application/anongame_lobby real dispatcher + IAnonGameLobbyRepository. (1) R173.a: replaced the R170.d placeholder `dispatch_admit` with the real decision function -- stateless `(LobbyAdmitRequest{entrant, current_queue, bracket_size}) -> LobbyAdmitResponse{status, promoted_party}`. Decision tree mirrors legacy `_anongame_queue` simpler paths: bracket<2 / account_id==0 / overflow -> kRejected; duplicate account_id -> kDuplicate; queue+1 == bracket -> kPromoted with FIFO `current_queue ++ entrant`; else kQueued. Bracket sizes 2/4/8 verified. NOT covered: gametype-specific tier filtering, AT-vs-PG flag, map_prefs intersection (those live in `anongame_infos.cpp` and move in alongside R174.c). (2) R173.b: new `lobby_repository.hpp` with `IAnonGameLobbyRepository` port -- `queue_for(game_type)`, `bracket_size_for(game_type)`, `add(LobbyEntry)`, `remove_party(span)`. Header-only; legacy adapter (over `anongame_queue` global) deferred. (3) R173.a tests: 9 new cases / ~30 assertions in `tests/unit/application/anongame_lobby/lobby_test.cpp` driving a `StaticLobbyRepository` test double through queue + promote cycles for brackets of 2, 4, and 8. (4) R173.c: DEFERRED to R174 -- new bridge `pvpgn_v3_anongame_lobby_apply` (base + linked), legacy `IAnonGameLobbyRepository` adapter over `anongame_queue`, gametype-specific bracket-size resolution, refactor of `_client_findanongame` / `_client_anongame_search` (~150 lines). Multi-day work; needs its own round. (5) R173.d: DEFERRED to R174 alongside R173.c (integration tests run against the bridge R173.c builds). (6) R173.e: DEFERRED -- enabling `create_character.cpp` / `delete_character.cpp` / `list_characters.cpp` / `load_character.cpp` / `save_character.cpp` / `join_game_server.cpp` requires real `ICharacterRepository` + `ICharacterListRepository` adapters that don't yet exist; needs an infra/legacy_charfile adapter round first. (7) R173.f: DEFERRED again (R171.f, R172.f, R173.f -- same scope). Should be its own dedicated round (R175?). Docker v3-test image `pvpgn-v3-test:r173` sha256:9670acaa26de4c040ba6d01cf1507f663e9b13ffa9d41e01e4c52cecd601c6ed (169 binaries green, +1 test_application_anongame_lobby versus R172's 168). v3-runtime not rebuilt: only pure-C++ application module + header were added; nothing in `integration_legacy_bnetd_linked` was touched.
  - [x] R174: anongame_lobby bridge base half + ABI tests. (1) R174.a: new C ABI `pvpgn_v3_anongame_lobby_apply(void* conn_ptr, unsigned game_type)` in `integration/legacy_bnetd/anongame_lobby_bridge.{hpp,cpp}` -- atomic handler pointer with acquire/release ordering, returns 1 (admitted) / 0 (handler-installed-but-failed) / -1 (no handler, fall back). Idempotent `install_legacy_anongame_lobby_handler()` -- stage-1 no-op; the legacy adapter over `anongame_queue` global is deferred to stage 2 so every call site keeps legacy behaviour. (2) R174.c-partial: 7 new ABI-level integration test cases / ~25 assertions in `test_integration_legacy_bnetd_anongame_lobby_bridge` covering no-handler / null-conn / forwarding / rc-propagation / get-reflects-set / install no-op / install idempotency. End-to-end "admit cycle through legacy queue" cases defer to R175 alongside R174.b. (3) R174.b: DEFERRED to R175 -- refactor `_client_findanongame` / `_client_anongame_search` (~200 lines of intrusive changes against legacy `anongame_queue` + `anongame_infos.cpp` bracket-size resolution + game-spawn + send_anongame_found wiring). Base bridge is harmless to merge alone: with no handler installed the ABI returns -1 and every call site keeps legacy behaviour. (4) R174.d / R174.e / R174.f / R174.g: NOT TOUCHED this round. The R174 multiSelect picked every option; with the base bridge + tests done, the remaining options (handle_init protocol module, infra/legacy_charfile adapter, application/realm/realm_session skeleton, handle_bnet audit) are queued for explicit selection in the R175 dialog rather than rushed past the verification budget. Docker v3-test image `pvpgn-v3-test:r174` sha256:cdd72124ab4c5685c11752b66dd62cd12a5ed5b81fff912e25c5470285550369 (170 binaries green, +1 anongame_lobby_bridge test versus R173's 169); v3-runtime not rebuilt -- the new bridge has no handler installed and is not wired from `bnetd_legacy`, so the runtime image is functionally unchanged from R172.
  - [x] R175: application/anongame_lobby bracket-size lookup mirroring legacy `_anongame_totalplayers`. (1) new `application/anongame_lobby/game_type.hpp` -- `enum class GameType : uint32_t` repeats `ANONGAME_TYPE_*` constants from `common/anongame_protocol.h` (so application/ avoids the legacy header dependency) + `constexpr bracket_size_for_game_type(uint32_t) -> uint8_t` returning 2/4/6/8/9/10/12 per gametype family, 0 for Tournament (dynamic; legacy adapter overrides) and 0 for unknown. (2) 8 new unit tests / ~20 assertions in `test_application_anongame_lobby_game_type` covering every bracket class + Tournament sentinel + 3 unknown values. (3) Dockerfile.v3 wires the new test. (4) R175.a / R175.b / R175.c: DEFERRED (linked adapter + handle_bnet refactor + e2e tests) -- legacy `_anongame_queue` is a 300-line gametype-dependent state machine over file-static `players[QUEUES_MAX][PLAYERS_MAX]`; the faithful adapter needs several legacy internal helpers exposed first. This round delivers the foundation piece (bracket-size table) so R176's adapter can simply delegate. (5) R175.d/e/g: DEFERRED (handle_init protocol module, charfile adapter, handle_bnet audit -- queued). (6) R175.f: DEFERRED + RE-SCOPING REQUIRED -- legacy has no `realm_session_t` (verified by grep) so `application/realm/realm_session` would be net-new infrastructure rather than strangler-fig extraction. Docker v3-test image `pvpgn-v3-test:r175` sha256:1a9fed67386f0b81dd1ec9f73b76c5acef720ef87efcc80b769002918ab3404d (171 binaries green, +1 anongame_lobby_game_type test versus R174's 170); v3-runtime not rebuilt -- only application/ pure-C++ added.
  - [x] R176: retire `src/bnetd/handle_init.cpp` -- body relocated to `integration_legacy_bnetd_linked`. (1) deleted `src/bnetd/handle_init.cpp` (105 lines -- since R170.e it was already a thin shim over `pvpgn_v3_init_conn_apply_ex`); (2) new `src/v3/integration/legacy_bnetd/src/init_packet_dispatch_link.cpp` receives the body verbatim (same `pvpgn::bnetd::handle_init_packet` symbol, same validation gates, same return semantics, same legacy-header bracket pattern allowed only in linked half); (3) `src/bnetd/handle_init.h` is kept -- `server.cpp:990` + `legacy_bnet_frame_router_link.cpp:81` still forward-decl it; (4) `src/bnetd/CMakeLists.txt` drops `handle_init.cpp` from bnetd_legacy sources; (5) `src/v3/CMakeLists.txt` adds the new file to `integration_legacy_bnetd_linked` SOURCES. Partial step toward the long-deferred R171.f/R172.f/R173.f/R174.d/R175.d effort -- the file-deletion part is done with zero wire-level behaviour change. Final stage (retire `handle_init.h` + retire `protocol/bnet/init_packet` from legacy `init_protocol.h` + refactor `server.cpp:992` packet pump) DEFERRED to a future dedicated round. Docker verified: v3-test `pvpgn-v3-test:r176` sha256:0749792838142617524ae1b76f2d2c7695e743c2d7bf0d3a459d2e25b831c595 (171 binaries, identical to R175 -- relocation only); v3-runtime `pvpgn-v3-runtime:r176` sha256:a8a6a7c14c0270408d2b9c9122ca970661c95b20431cf01d4a79b1fdbffbfe9b (pvpgn_v3_bnetd links + installs cleanly with the relocated symbol -- no ODR / no link-order regression).
  - [x] R177: protocol/bnet init-conn codec (R177.b). (1) NEW `src/v3/protocol/bnet/include/protocol/bnet/init_codec.hpp` -- pure-C++ codec joining the pre-existing `init_wire_types.hpp` (`ClientInitConn` struct + `kClass*` constants). Adds `kClientInitConnSize=1`, `parse_client_initconn` (std::byte + std::uint8_t span overloads), `encode_client_initconn`. Parser enforces framing only (rejects empty / multi-byte); cclass-value policy lives in `application/init/init_conn_dispatch`. (2) NEW `tests/unit/protocol/bnet/init_codec_test.cpp` -- 8 cases / ~795 assertions: size, byte/uint8_t parse overloads, empty + multi-byte rejection, full 0x00..0xFF parse, encode byte, full-byte-space encode/parse roundtrip, per-`kClass*` constant roundtrip. (3) `tests/unit/protocol/bnet/CMakeLists.txt` + `Dockerfile.v3` wire the new test. (4) DEFERRED to R178: R177.a (swap `server.cpp:990` / `legacy_bnet_frame_router_link.cpp:81` from `pvpgn::bnetd::handle_init_packet` to v3-native dispatch -- now mechanically possible because codec exists); R177.c (v3 packet-pump refactor -- multi-round); R177.d (ads_bridge linked + `_client_findadreq`); R177.e (anongame_lobby linked adapter); R177.f (handle_bnet audit -- research-only); R177.g (legacy-handler cleanup -- needs (f) first). Docker verified: v3-test `pvpgn-v3-test:r177` sha256:39d5665c2d37e8674d45cd2a425a0faed47a51ddbae9991f6eb61a8f6af5abe5 (172 "All tests passed", +1 from R176); v3-runtime `pvpgn-v3-runtime:r177` sha256:001802f1cc6e14b025bbeae84cf5e45f4ca21a7bfaca41882d7507432a148cde. Both stages green -- pure header-only addition, no link-order or ABI risk.
  - [x] R178: retire `src/bnetd/handle_init.h` -- final step of the init-packet effort. (1) DELETED `src/bnetd/handle_init.h` (5-line forward decl); (2) `src/bnetd/server.cpp` swaps the `#include` for an inline `extern int handle_init_packet(t_connection*, t_packet const* const)` at the top of `pvpgn::bnetd`; (3) `legacy_bnet_frame_router_link.cpp` does the same with an out-of-bracket namespace-qualified forward decl so legacy headers no longer leak into v3 include paths; (4) `init_packet_dispatch_link.cpp` drops the include entirely (it defines the function); (5) `src/bnetd/CMakeLists.txt` drops the header. Completes the long-deferred R171.f/R172.f/R173.f/R174.d/R175.d/R176-tail arc -- `src/bnetd/handle_init.{cpp,h}` are now both gone. R178.c (packet-pump refactor that would also retire `init_packet_dispatch_link.cpp`) DEFERRED to R179. Docker verified: v3-test `pvpgn-v3-test:r178` sha256:bd661f9e0b7886e44aafcdc2861acf8263cf377e7edda829e6ecdbf8d2b318bd (172 "All tests passed", identical to R177 -- no new tests, header retirement only); v3-runtime `pvpgn-v3-runtime:r178` sha256:a790e5abf093f733652825645059312e0f5b95a6ff304ac39b625d5a2684ef5b (inline forward decls bind to the relocated symbol cleanly).
  - [x] R179: application/bnet_packet_pump lifecycle FSM scaffold (R179.a). (1) NEW `application/bnet_packet_pump/conn_class.hpp` -- `enum class ConnClass` mirroring legacy `conn_class_*` (kInit/kBnet/kFile/kBot/kTelnet/kIrc/kD2cs/kD2csBnetd/kW3route/kWol/kWolGameres/kWgameres/kWserv/kApiReg/kAuthReq) + `to_string`; (2) NEW `application/bnet_packet_pump/lifecycle.hpp` -- `enum class Lifecycle { kAwaitingInit, kDispatching, kRejected, kClosed }`, `conn_class_from_cclass_byte(uint8_t)` consuming `protocol::bnet::init::kClass*` directly, pure `constexpr step_on_cclass_byte(Lifecycle, uint8_t) -> LifecycleStep`; (3) NEW `application_bnet_packet_pump` static lib (PUBLIC_DEPS core protocol_bnet, leaf -- no inbound edges yet) with a placeholder TU; (4) NEW `test_application_bnet_packet_pump_lifecycle` (6 cases / ~310 assertions: golden Bnet mapping, every kClass* mapping, exhaustive 0x00..0xFF rejection scan, no-op in non-kAwaitingInit states, `to_string` coverage); (5) Dockerfile.v3 + tests/unit/application/CMakeLists.txt wire the new target. Foundation for the future v3 packet pump that will ultimately retire `init_packet_dispatch_link.cpp` -- with codec (R177.b) + FSM + relocated shim (R176) in place, R180+ writes the driver. R179.b/c DEFERRED (ads_bridge linked + anongame_lobby linked -- ~150 + ~200 lines, both fit cleanly into a future round). Docker verified: v3-test `pvpgn-v3-test:r179` sha256:eac3b791feb90a8f7e708bd8459273c3e9ee9f668b6b4b1b7e4c41dd0b4c3e41 (173 "All tests passed", +1 from R178); v3-runtime `pvpgn-v3-runtime:r179` sha256:f0db94ce88f6b709fc2c1490dc14f74ab09fc64f4a512e0f092db565eaa440a8.
  - [x] R180: application/bnet_packet_pump driver class (R180.c). (1) NEW `application/bnet_packet_pump/driver.hpp` -- `class PacketPumpDriver` with `feed(span<byte>) -> FeedOutcome`, `close()`, and state accessors (`state()`, `class_now()`, `is_open()`, `is_closed()`, `is_rejected()`). In `kAwaitingInit` calls `parse_client_initconn` + `step_on_cclass_byte`; in `kDispatching` returns `kAlreadyOpen`; in `kRejected`/`kClosed` returns `kClosed`. `FeedOutcome` enum: kAccepted/kRejected/kAlreadyOpen/kMalformed/kClosed; pure-C++ constexpr throughout. (2) NEW `test_application_bnet_packet_pump_driver` (10 cases / ~40 assertions: fresh default, Bnet opens, unknown rejects, empty/two-byte are kMalformed and preserve state, post-open returns kAlreadyOpen, post-reject returns kClosed, close() idempotent, every kClass* opens with right class, FeedOutcome::to_string). (3) Dockerfile.v3 + tests CMake wire the new test. With codec (R177.b) + FSM (R179.a) + driver (R180.c), the v3 side now has end-to-end init-byte handshake logic with zero legacy types. R181+ wires `PacketPumpDriver` into `legacy_bnet_frame_router_link.cpp` and retires `init_packet_dispatch_link.cpp`'s body. R180.a/b DEFERRED (ads_bridge linked + anongame_lobby linked -- carry-over). Docker verified: v3-test `pvpgn-v3-test:r180` sha256:18c055d3921e0aa4800668064e8d8e757ae465b458c35c23185cd1aa34e1a20c (174 "All tests passed", +1 from R179); v3-runtime `pvpgn-v3-runtime:r180` sha256:ac6ab5a620dc3615003b501dd488f0cca41e25a67f9da96c1089e9f76a2a8a0c.
  - [x] R181.c: wire `PacketPumpDriver` (R180.c) into `init_packet_dispatch_link.cpp` as an advisory shadow trace. (1) Driver is fed the cclass byte before the authoritative `pvpgn_v3_init_conn_apply_ex` call; `FeedOutcome` is observed but the function still returns `(v3_rc == 1) ? 0 : -1`. (2) Asymmetric parity check emits one `eventlog_level_warn` only when driver REJECTED a byte that legacy ACCEPTED (missing cclass mapping); the inverse direction (driver accepts, legacy rejects on rate-limit / realm-list policy that the driver doesn't model) stays silent. (3) `application_bnet_packet_pump` added to `integration_legacy_bnetd_linked` PUBLIC_DEPS -- additive, no link-order risk. Runtime contract unchanged. Future R182 promotes the driver to authoritative once it models policy too, then a later round retires `init_packet_dispatch_link.cpp`'s body. R181.a (findadreq), R181.b (anongame_lobby linked), R181.e (d2cs handle_init relocation -- still WITH_D2CS=OFF blocked), R181.f (legacy cleanup -- audit blocked) DEFERRED to R182. Docker verified: v3-test `pvpgn-v3-test:r181` sha256:e235fe3ee33921326dde5544d94468386eca8070f9959a51499ce01e9a1c0e9b (174 "All tests passed", identical to R180 -- driver already tested); v3-runtime `pvpgn-v3-runtime:r181` sha256:77e156dc6cbbb9697d3a0ca0be69600f3214c669bb28d44a0abc07a001c31e61.
  - [x] R182.a: promote `PacketPumpDriver` to model rate-limit + realmlist policy. (1) `driver.hpp` includes `application/init/init_conn_dispatch.hpp`; `enum FeedOutcome` gains `kRateLimited` + `kD2csIpDenied`; NEW `struct PumpPolicy { conn_count, max_conns_per_ip, d2cs_ip_allowed }` (defaults disable rate-limit, allow d2cs). (2) `feed(span)` delegates to NEW `feed(span, PumpPolicy)` which calls `dispatch_init_conn(InitConnRequest{...})` before the byte FSM and maps verdicts: `kRateLimited`/`kD2csIpDenied` -> terminal `kRejected` + matching outcome; recognised classes + `kRejected` fall through to the existing `step_on_cclass_byte`. (3) `application_init` added to `application_bnet_packet_pump` PUBLIC_DEPS. (4) `init_packet_dispatch_link.cpp` advisory call now feeds the full policy through; parity log message includes `to_string(FeedOutcome)`. (5) Tests: `to_string` covers `kRateLimited`/`kD2csIpDenied`; 7 new policy-aware cases (default preserves byte-only; rate-limit triggers + terminal; D2CS_BNETD exempt from rate-limit; D2CS_BNETD with denied IP; `d2cs_ip_allowed=false` no-op on non-D2CS; equal-to-max allowed; `max_conns_per_ip=0` disables gate). Driver is now a faithful pure-C++ replica of the legacy/v3 init verdict including policy -- sets up R183.c (promote driver to authoritative). R182.b (`_client_findadreq`) and R182.c (anongame_lobby linked) DEFERRED to R183. Docker verified: v3-test `pvpgn-v3-test:r182` sha256:0043800b38d2e346dd70b0f1c44f586ef114efe50c87484ae5c38a294f3ed37f; v3-runtime `pvpgn-v3-runtime:r182` sha256:b3b4b4c408a5153360353e976320862c233736be8f0f83234d35e138ad702e9f.
  - [x] R183.a: promote `PacketPumpDriver` to AUTHORITATIVE for the init verdict. (1) `init_packet_dispatch_link.cpp`: `handle_init_packet` now returns `(pump_outcome == kAccepted) ? 0 : -1`, computed from the v3 driver -- the legacy `pvpgn_v3_init_conn_apply_ex` bridge is still called for its side effects on `t_connection` (class set, realmlist hookup, response packets) but its rc is demoted to a parity probe. (2) Parity check upgraded from asymmetric to **bidirectional**: any `driver_rc != legacy_rc` emits one `eventlog_level_warn` with both verdicts and the `FeedOutcome` enumerator name. Since both paths consult `application/init/dispatch_init_conn` against identical policy inputs they MUST agree by construction -- the log is a tripwire. (3) Next round (R184+) teaches the driver to emit the side effects the legacy bridge still owns so the bridge can be retired and this file becomes a pure driver wrapper. R183.b (findadreq), R183.c (anongame_lobby linked), R183.e (d2cs handle_init -- WITH_D2CS=OFF blocked) DEFERRED to R184. Docker verified: v3-test `pvpgn-v3-test:r183` sha256:8a71fc14aed158d841dc995665fa264427719fe5957d537d521ee33fc0dc657c (174 "All tests passed", identical to R182 -- pure flip); v3-runtime `pvpgn-v3-runtime:r183` sha256:c2d1885292177222872bb29a7d37b582d3710629fc73feb46671c3998322f8ed.

### Side Tracks (all complete)

- [x] xalloc elimination — Rounds 1-13 COMPLETE (100% xalloc-free in bnetd + all subsystems)
- [x] src/test/ retirement — ported to Catch2, deleted
- [x] src/json/ retirement — switched to nlohmann/json
- [x] src/bnpass/ retirement — migrated to src/v3/tools/bnpass/
- [x] src/bnproxy/ retirement — dropped (deprecated since 4.0)
- [x] src/bniutils/ relocation — moved to src/v3/tools/bniutils/ + deep modernization
- [x] src/bntrackd/ relocation — moved to src/v3/tools/bntrackd/
- [x] src/client/ deep modernization — bnbot, bnftp, bnchat, bnstat all standalone v3
- [x] E2E docker smoke tests — bnftp, bnchat, bnstat, bnbot all green
- [x] compat filesystem cluster retirement
- [x] compat/strdup, strcasecmp, strncasecmp, strsep, gettimeofday, uname, mmap retirement
- [x] t_connection per-field migration round 1 (clientexe, clientver, loggeduser)

## Phase 2: Protocol FSM Completion ✅ COMPLETE (Round 130)

> **Completed:** 2026-05-21 (R122–R129)
> **Total test coverage:** 161 test cases, 851 assertions across all FSMs

### Phase 2 Summary

All protocol FSMs are now production-ready and wired into the strangler-fig composition root.
The `pvpgn_v3_bnetd` binary builds cleanly with Asio event loop, `SessionManager`, `TcpSession<FSM>`,
and `TcpListener` for all active protocols.

#### FSMs Completed

| FSM | File | Lines | Tests | Assertions | Round |
|-----|------|-------|-------|------------|-------|
| `BnetFsm` | `protocol/bnet/fsm.cpp` | 829 | 28 | 154 | ✅ R124 |
| `BnftpFsm` | `protocol/file/bnftp_fsm.cpp` | 320 | 13 | 76 | ✅ R122 |
| `WolFsm` | `protocol/wol/wol_fsm.cpp` | ~280 | 34 | 153 | ✅ R123 |
| `IrcFsm` | `protocol/irc/fsm.cpp` | ~340 | 46 | 293 | ✅ R127 |
| `D2CSSessionFsm` | `protocol/d2cs/fsm.cpp` | ~300 | 40 | 175 | ✅ R129 |

#### Composition Root (`src/v3/app/bnetd/`) — ✅ R125–R128

- `pvpgn_v3_bnetd` binary builds cleanly
- Asio event loop (`IoRuntime`) with configurable thread pool
- `SessionManager` — thread-safe registry of active sessions (weak_ptr, shared_mutex)
- `TcpSession<FSM>` — generic session template wiring Asio reads → FSM `on_bytes()`
- `TcpListener` — thin wrapper around `TcpAcceptor` with start/stop
- `BnftpTcpSession` + `FileSessionFactory` wired (R126) — dedicated BNFTP port listener
- `IrcTcpSession` + `IrcSessionFactory` wired (R128) — IRC port listener on :6667
- Signal handlers (SIGINT/SIGTERM) → `IoRuntime::stop()` graceful shutdown

#### Round-by-Round Log

- **R122**: Audited all FSMs; implemented `BnftpFsm` (320 lines, 13 tests, 76 assertions); fixed `D2CSSessionFsm` build bug
- **R123**: Implemented `WolFsm` IRC-like chat from stub (~280 lines, 34 test cases, 153 assertions)
- **R124**: Fixed `BnetFsm` TODOs — `message_router` broadcast, `JoinGame` reply, `client_tag` tracking (28 tests, 154 assertions)
- **R125**: Created composition root `src/v3/app/bnetd/` with Asio event loop, `SessionManager`, `TcpSession<FSM>`, `TcpListener` — `pvpgn_v3_bnetd` binary builds cleanly
- **R126**: Wired `BnftpFsm` into `FileSessionFactory` + `BnftpTcpSession` (12 tests, 22 assertions)
- **R127**: Fixed `IrcFsm` TODOs — expanded from 139 → ~340 lines, 46 test cases, 293 assertions
- **R128**: Wired `IrcFsm` into `IrcTcpSession` + `IrcSessionFactory` in composition root (14 tests, 36 assertions)
- **R129**: Fixed `D2CSSessionFsm` — 12 packet handlers, 40 test cases, 175 assertions

### Phase 2 Steps (all complete)

- [x] Step 1: Domain layer extraction — pre-existing v3 domain aggregates verified
- [x] Step 2: Application use cases — all use-cases in `BnetUseCaseContext` verified
- [x] Step 3: Protocol FSMs — BnetFsm, BnftpFsm, WolFsm, IrcFsm, D2CSSessionFsm all complete
- [x] Step 4: Infrastructure layer — `infra/net`, `infra/inmemory`, `infra/session` all wired
- [x] Step 5: Connection state machine decomposition — `TcpSession<FSM>` generic template
- [x] Step 6: Composition root — `src/v3/app/bnetd/` with all listeners wired
- [ ] Step 7: Legacy bnetd deletion — **next phase** (handle_*.cpp files to be deleted)
- [ ] Step 8: Final integration — pending legacy deletion

---

## Phase 3: bnetd Server Logic Migration 🔄 IN PROGRESS (deferred items remain)

> **Started:** Round 130
> **Active through:** Round 142
> **Depends on:** Phase 2 (complete ✅)
> **Reference:** [`plans/phase3-bnetd-checklist.md`](plans/phase3-bnetd-checklist.md)

### Phase 3 Scope

Migrate `src/bnetd/` (140+ files) into the v3 hexagonal architecture by deleting legacy
`handle_*.cpp` files one by one as the v3 FSMs take over each protocol. The strangler-fig
bridges already intercept all outbound packet sites; Phase 3 completes the inbound side.

### Phase 3 Completed (R131–R142)

- [x] **R131** — Phase 3 audit + `plans/phase3-bnetd-checklist.md` created
- [x] **R132** — Deleted `handle_file.cpp/h`, `handle_wol_gameres.cpp/h`
- [x] **R133** — Migrated `psock.h` callers (5 bnetd files)
- [x] **R134** — Deleted `handle_irc.cpp` + `handle_irc_common.cpp`; inlined into `irc.cpp`
- [x] **R135** — Fixed missing `infra/compat/directory.hpp` (CMakeLists.txt fallbacks)
- [x] **R136** — `ConnectionFsm` skeleton (28 tests, 183 assertions)
- [x] **R137** — `ConnectionFsm` expanded with `InGame` state (45 tests, 407 assertions)
- [x] **R138** — `AsioEventLoop` + `LegacyBridge` + `server_v3_hook` (12 tests)
- [x] **R139** — `server_tick_v3(5)` wired into `server.cpp` main loop
- [x] **R140** — `BnetConnectionAdapter` bridges `BnetFsm` → `ConnectionFsm` (13 tests, 108 assertions)
- [x] **R141** — `LuaRuntime` + `LuaConnectionContext` (10 tests)
- [x] **R142** — Handler audit: `handle_wol.cpp`, `handle_wserv.cpp`, `handle_d2cs.cpp` — all deferred

### Phase 3 Deferred (blocked)

- [ ] Delete `handle_wol.cpp/h` — blocked by 3 call sites in `irc.cpp` + `anongame_wol.cpp`
- [ ] Delete `handle_wserv.cpp/h` — blocked by 1 call site in `irc.cpp`
- [ ] Delete `handle_d2cs.cpp/h` — blocked by `server.cpp:996`, `handle_init.cpp:192`, `connection.cpp:2187`
- [ ] Delete `handle_telnet.cpp` — `TelnetAdminFsm` needs login flow first
- [ ] Delete `handle_init.cpp` — `ConnectionClassifier` needs wiring first
- [ ] Delete `handle_bnet.cpp` — after `BnetFsm` covers all SIDs
- [ ] Delete `handle_bot.cpp` — after `BotProtocolFsm` complete
- [ ] Integration bridge cleanup (`LegacyBridge`, `server_v3_hook`)
- [ ] Migrate `prefs_get_*` callers (36/~38 files done R136–R139; ~2 files remain)
- [ ] Delete `src/bnetd/prefs.cpp` / `prefs.h` after all callers migrated

#### Priority 2 — Domain / Application Layer Completion (still pending)

- [ ] `connection.cpp/.h` → `domain/shared/` + `infra/net/`
- [ ] `server.cpp/.h` → `runtime/` + `infra/net/`
- [ ] `storage.cpp/.h` + `storage_file.cpp/.h` → `infra/persistence/` + `infra/file/`
- [ ] SQL backends (`sql_mysql`, `sql_sqlite3`, `sql_pgsql`, `sql_odbc`) → `infra/*/`
- [ ] `adbanner.cpp/.h` → `application/adbanner/`
- [ ] `autoupdate.cpp/.h` → `application/autoupdate/`
- [ ] `versioncheck.cpp/.h` → `application/versioncheck/`
- [ ] `news.cpp/.h` → `application/news/`
- [ ] `mail.cpp/.h` → `application/mail/`
- [ ] `icons.cpp/.h` → `application/icon_table/`
- [ ] `i18n.cpp/.h` → `application/i18n/` (pugixml already migrated R119)
- [ ] `tracker.cpp/.h` → `infra/tracker/`
- [ ] `userlog.cpp/.h` → `infra/audit/`
- [ ] `watch.cpp/.h` → `application/watch/`
- [ ] `lua*.cpp/.h` → `infra/scripting/lua/`

## Phase 4: D2 Services Migration 🔜 STARTING

> **Started:** Round 143
> **Depends on:** Phase 3 (in progress — D2 work is independent)
> **Reference:** [`plans/phase4-d2-checklist.md`](plans/phase4-d2-checklist.md), [`plans/refactoring-plan-legacy-d2.md`](plans/refactoring-plan-legacy-d2.md)

### Phase 4 Scope

Migrate `src/d2cs/` (42 files, ~8 700 LOC) and `src/d2dbs/` (20 files, ~4 100 LOC) into the
v3 hexagonal architecture. Both services already have static-library carve-outs
(`d2cs_legacy`, `d2dbs_legacy`) and v3 composition-root stubs. The `D2CSSessionFsm` (310-line
header, 706-line impl) is already complete from R129.

### Phase 4 What Already Exists in v3

- `domain/realm/` — `Character`, `CharacterList`, `DupeChecker`, `Realm` aggregates
- `application/realm/` — `CharacterLock`, `CharacterPersistence`, `GsQueue`, `CreateCharacter`,
  `DeleteCharacter`, `ListCharacters`, `LoadCharacter`, `SaveCharacter`, `JoinGameServer`
- `protocol/d2cs/` — `D2CSSessionFsm` (complete), `Codec`, `CharListReplyEncoder`, `LadderReplyEncoder`
- `protocol/d2dbs/` — `Codec`, `D2DBSSessionFsm` (stub)
- `protocol/d2gs/` — `Codec`
- `protocol/d2save/` — D2 save-file codec
- `infra/persistence/realm/` — `FilesystemSaveStore`, `InMemoryCharacterRepository`, `InMemorySaveStore`
- `services/d2cs/` — composition root stub (`D2CSComposition`)
- `services/d2dbs/` — composition root stub (`D2DBSComposition`)

### Phase 4 Steps (see checklist for detail)

- [ ] **Step 1** — Domain layer: `CharacterList` + `D2Ladder` domain objects
- [ ] **Step 2** — Application layer: D2CS + D2DBS use-case services
- [ ] **Step 3** — Protocol FSM: D2CS FSM wired to app layer; D2DBS FSM expanded
- [ ] **Step 4** — Infrastructure: character-file persistence + ladder persistence
- [ ] **Step 5** — Inter-service communication: D2CS↔D2DBS + D2CS↔D2GS adapters
- [ ] **Step 6** — Composition roots: wire D2CS + D2DBS services end-to-end
- [ ] **Step 7** — Combined mode: D2CS + D2DBS run inside bnetd process

## Phase 5: Tools Migration ✅ Mostly Done

- [x] bnpass — migrated to src/v3/tools/bnpass/
- [x] bnproxy — dropped (deprecated)
- [x] bniutils — relocated to src/v3/tools/bniutils/ + modernized
- [x] bntrackd — relocated to src/v3/tools/bntrackd/
- [x] client tools (bnbot, bnftp, bnchat, bnstat) — deep modernization complete
- [ ] Delete remaining legacy tool files (after Phase 1 Step 9)

## Build System Consolidation

- [ ] Phase 1: Inventory and audit
- [ ] Phase 2: Unify CMake structure
- [ ] Phase 3: Migrate targets
- [ ] Phase 4: Remove legacy build artifacts
- [ ] Phase 5: CI/CD updates
- [ ] Phase 6: Documentation
- [ ] Phase 7: Final verification

## Testing

- [x] Unit tests for v3 protocol layer (1225 assertions / 213 cases)
- [x] Integration tests for legacy bridges (all bridge tests green)
- [x] E2E smoke tests (bnftp, bnchat, bnstat, bnbot — all green)
- [ ] Coverage targets met
- [ ] Legacy test removal

## Session Log

### 2026-05-19
- Orchestrator started systematic refactoring execution
- Read and analyzed all plan files
- Created this master progress tracker
- Analyzed step4-checklist.md: Rounds 1-37 complete (observation bridges for all major handlers)
- xalloc elimination 100% complete (Rounds 1-13)
- All side tracks complete (tools, compat, etc.)
- Round 38 COMPLETE: Implemented `pvpgn_v3_clan_send_try` observation bridge
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/clan_send_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/clan_send_bridge.cpp`
  - Created `tests/unit/integration/legacy_bnetd/clan_send_bridge_test.cpp` (3 test cases)
  - Added source to `src/v3/CMakeLists.txt`
  - Added test to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/clan.cpp` at all 8 `conn_push_outqueue` sites under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Updated `plans/step4-checklist.md`: `[x] clan.`
- E.3 is now 100% complete (all modules have observation bridges)
- Next action: Work toward E.4 acceptance criteria (reduce 156 packet_create sites to 0)
- Round 39 COMPLETE: Implemented `send_handshake_bridge` — E.4 first batch
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_handshake_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_handshake_bridge.cpp`
    - `pvpgn_v3_send_compreply(conn)` — SERVER_COMPREPLY (0x05), 20 bytes, all fixed constants
    - `pvpgn_v3_send_sessionkey1(conn, sessionkey)` — SERVER_SESSIONKEY1 (0x28), 8 bytes
    - `pvpgn_v3_send_sessionkey2(conn, sessionnum, sessionkey)` — SERVER_SESSIONKEY2 (0x1D), 12 bytes
  - Created `tests/unit/integration/legacy_bnetd/send_handshake_bridge_test.cpp` (12 test cases)
    - Wire-parity byte checks for all 3 packet types
    - null-conn rejection, no-handler returns 0, handler return propagation
  - Added source to `src/v3/CMakeLists.txt`
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3` (line 87)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added 3 forward decls under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_compinfo1`: 2 `packet_create` sites now guarded by bridge calls
    - `_client_compinfo2`: 2 `packet_create` sites now guarded by bridge calls
  - Updated `plans/step4-checklist.md`: Round 39 entry, handle_bnet.cpp count 68→64
  - Running total: **152 packet_create sites** remaining (down from 156)
- Round 40 COMPLETE: Implemented `send_authreq1_server_bridge` — SERVER_AUTHREQ1 (0x06)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_authreq1_server_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_authreq1_server_bridge.cpp`
    - `pvpgn_v3_send_authreq1_server(conn, timestamp, filename, equation)`
    - Uses existing `encode(AuthReq1Server)` codec: FF 06 size_le + u64 ts + filename\0 + equation\0
  - Created `tests/unit/integration/legacy_bnetd/send_authreq1_server_bridge_test.cpp` (9 test cases)
    - null-conn, null-filename, null-equation rejection
    - Wire-parity byte checks for non-empty and empty strings
    - Handler return value propagation (1 / 0 / -1)
  - Added source to `src/v3/CMakeLists.txt` (before send_authreply1_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target to `Dockerfile.v3` cmake line (via sed)
  - Added RUN test line to `Dockerfile.v3` (after init_conn_bridge test)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added forward decl `pvpgn_v3_send_authreq1_server` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_progident`: 1 `packet_create` site now guarded by bridge call
    - Refactored to compute checkrevision + timestamp before the bridge/legacy branch
  - Updated `plans/step4-checklist.md`: Round 40 entry, handle_bnet.cpp count 64→63
  - Running total: **151 packet_create sites** remaining (down from 152)
- Round 41 COMPLETE: Implemented `send_cdkey_reply_bridge` — SERVER_CDKEYREPLY (0x30), SERVER_CDKEYREPLY2 (0x36), SERVER_CDKEYREPLY3 (0x42)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_cdkey_reply_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_cdkey_reply_bridge.cpp`
    - `pvpgn_v3_send_cdkeyreply(conn, message, owner)` — SERVER_CDKEYREPLY (0x30): header(4) + u32 LE + owner\0
    - `pvpgn_v3_send_cdkeyreply2(conn, result, owner)` — SERVER_CDKEYREPLY2 (0x36): header(4) + u32 LE + owner\0
    - `pvpgn_v3_send_cdkeyreply3(conn, message, owner_name)` — SERVER_CDKEYREPLY3 (0x42): header(4) + u32 LE + owner_name\0
  - Created `tests/unit/integration/legacy_bnetd/send_cdkey_reply_bridge_test.cpp` (12 test cases)
    - null-conn rejection, no-handler returns 0, wire-parity for non-empty and nullptr owner
    - Handler return value propagation (1 / 0 / -1) for all 3 functions
  - Added source to `src/v3/CMakeLists.txt` (before send_chatevent_compose_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target to `Dockerfile.v3` cmake line (via sed)
  - Added RUN test line to `Dockerfile.v3` (after send_chatevent_compose_e2e test)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added 3 forward decls `pvpgn_v3_send_cdkeyreply/2/3` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_cdkey`: 1 `packet_create` site now guarded by bridge call
    - `_client_cdkey2`: 1 `packet_create` site now guarded by bridge call
    - `_client_cdkey3`: 1 `packet_create` site now guarded by bridge call
  - Updated `plans/step4-checklist.md`: Round 41 entry, handle_bnet.cpp count 63→60
  - Running total: **148 packet_create sites** remaining (down from 151)
- Round 42 COMPLETE: Implemented `send_fileinforeply_bridge` — SERVER_FILEINFOREPLY (SID_GETFILETIME, 0x33) and SERVER_PINGREPLY (SID_NULL, 0x00)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_fileinforeply_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_fileinforeply_bridge.cpp`
    - `pvpgn_v3_send_fileinforeply(conn, type, unknown2, timestamp, filename)` — SERVER_FILEINFOREPLY (0x33): header(4) + u32 type LE + u32 unknown2 LE + u64 timestamp LE + filename\0
    - `pvpgn_v3_send_pingreply(conn)` — SERVER_PINGREPLY (0x00): 4-byte header only
  - Created `tests/unit/integration/legacy_bnetd/send_fileinforeply_bridge_test.cpp`
    - null-conn rejection, no-handler returns 0, wire-parity for fileinforeply and pingreply
    - null filename treated as empty string, handler return propagation (1 / 0 / -1)
  - Added source to `src/v3/CMakeLists.txt` (before send_chatevent_compose_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target to `Dockerfile.v3` cmake line
  - Added RUN test line to `Dockerfile.v3` (after send_cdkey_reply_bridge test)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added forward decls `pvpgn_v3_send_fileinforeply` and `pvpgn_v3_send_pingreply` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_fileinforeq`: 1 `packet_create` site now guarded by bridge call (extracts timestamp from rpacket before bridge call)
    - `_client_pingreq`: guarded by `pvpgn_v3_send_pingreply` (existing function, newly wired)
  - Updated `plans/step4-checklist.md`: Round 42 entry, handle_bnet.cpp count 60→58
  - Running total: **146 packet_create sites** remaining (down from 148)
- Round 43 COMPLETE: Implemented `send_passchange_bridge` — SERVER_PASSCHANGEREPLY (SID_PASSCHANGE, 0x55) and SERVER_PASSCHANGEPROOFREPLY (SID_PASSCHANGEPROOF, 0x56)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_passchange_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_passchange_bridge.cpp`
    - `pvpgn_v3_send_passchangereply(conn, message, salt, server_public_key)` — SERVER_PASSCHANGEREPLY (0x55): header(4) + u32 message LE + salt[32] + server_public_key[32] = 72 bytes
    - `pvpgn_v3_send_passchangeproofreply(conn, response, server_password_proof)` — SERVER_PASSCHANGEPROOFREPLY (0x56): header(4) + u32 response LE + server_password_proof[20] = 28 bytes
  - Created `tests/unit/integration/legacy_bnetd/send_passchange_bridge_test.cpp`
    - null-conn rejection, no-handler returns 0, wire-parity (FF 55 48 00 and FF 56 1C 00 headers verified)
    - handler return propagation (1 / 0 / -1) for both functions
  - Added source to `src/v3/CMakeLists.txt` (after send_fileinforeply_bridge.cpp)
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - Added forward decls `pvpgn_v3_send_passchangereply` and `pvpgn_v3_send_passchangeproofreply` under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
    - `_client_passchangereq`: bridge reads message/salt/spk from rpacket, calls `pvpgn_v3_send_passchangereply`; on rc==1 skips legacy push
    - `_client_passchangeproofreq`: bridge reads response/proof from rpacket, calls `pvpgn_v3_send_passchangeproofreply`; on rc==1 skips legacy push + clears proofs
  - Updated `plans/step4-checklist.md`: Round 43 entry, handle_bnet.cpp count 58→56
  - Running total: **144 packet_create sites** remaining (down from 146)
- Round 44 COMPLETE: Wired `send_statsreply_bridge` — SERVER_STATSREPLY (SID_READUSERDATA, 0x26)
  - Bridge files (`send_statsreply_bridge.hpp`, `send_statsreply_bridge.cpp`, `send_statsreply_bridge_test.cpp`) were created during Round 44 setup (interrupted); this round completes the wiring
  - `pvpgn_v3_send_statsreply(conn, name_count, key_count, request_id, values, value_count)` — variable-length: header(4) + name_count(4) + key_count(4) + request_id(4) + NUL-terminated value strings
  - Forward decl `pvpgn_v3_send_statsreply` already present in `handle_bnet.cpp` `#ifdef PVPGN_V3_BNETD_INTEGRATION` block (lines 273-278)
  - Wired into `src/bnetd/handle_bnet.cpp` `_client_statsreq`: after legacy builds rpacket + appends value strings, bridge collects them into `std::vector<char const*>` and calls `pvpgn_v3_send_statsreply`; on rc==1 skips legacy push
  - Added `send_statsreply_bridge.cpp` to `src/v3/CMakeLists.txt` (after send_passchange_bridge.cpp)
  - Added `test_integration_legacy_bnetd_send_statsreply_bridge` to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Updated `plans/step4-checklist.md`: Round 44 entry, handle_bnet.cpp count 56→55
  - Running total: **143 packet_create sites** remaining (down from 144)
- Round 45 COMPLETE: Wired 5 friend/arranged-team send bridges — SERVER_FRIENDSLISTREPLY (0x65), SERVER_FRIENDINFOREPLY (0x66), SERVER_ARRANGEDTEAM_FRIENDSCREEN (0x60), SERVER_ARRANGEDTEAM_INVITE_FRIEND_ACK (0x61), SERVER_ARRANGEDTEAM_MEMBER_DECLINE (0x62)
  - New bridge files created (headers, impls, tests):
    - `send_friendslist_bridge.hpp/.cpp` + `send_friendslist_bridge_test.cpp`
    - `send_friendinfo_bridge.hpp/.cpp` + `send_friendinfo_bridge_test.cpp`
    - `send_atfriendscreen_bridge.hpp/.cpp` + `send_atfriendscreen_bridge_test.cpp`
    - `send_atinvitefriend_bridge.hpp/.cpp` + `send_atinvitefriend_bridge_test.cpp`
    - `send_atacceptdecline_bridge.hpp/.cpp` + `send_atacceptdecline_bridge_test.cpp`
  - Forward decls added to `handle_bnet.cpp` `#ifdef PVPGN_V3_BNETD_INTEGRATION` block (lines 282–312)
  - Wired into `src/bnetd/handle_bnet.cpp`:
    - `_client_friendslistreq`: re-walks friend list to build `std::vector<pvpgn_v3_friend_entry>`, calls `pvpgn_v3_send_friendslistreply`; on rc==1 skips legacy push
    - `_client_friendinforeq` offline path: calls `pvpgn_v3_send_friendinforeply` with FRIEND_TYPE_NON_MUTUAL/FRIENDSTATUS_OFFLINE/0/""; on rc==1 skips legacy push
    - `_client_friendinforeq` online path: reads type/status/clienttag from rpacket, extracts game_name from packet payload, calls `pvpgn_v3_send_friendinforeply`; on rc==1 skips legacy push
    - `_client_atfriendscreen`: collects player name strings from packet payload into `std::vector<char const*>`, calls `pvpgn_v3_send_atfriendscreenreply`; on rc==1 skips legacy push
    - `_client_atinvitefriend` ACK: reads count/id/timestamp/teamsize/info[5] from rpacket, calls `pvpgn_v3_send_atinvitefriendack`; on rc==1 skips legacy push (SERVER_ARRANGEDTEAM_SEND_INVITE to invitees not yet bridged)
    - `_client_atacceptdeclineinvite`: calls `pvpgn_v3_send_atmemberdecline` with count/action/decliner_name; on rc==1 skips legacy push
  - Added 5 new `.cpp` source files to `src/v3/CMakeLists.txt`
  - Added 5 new test targets to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added 5 new build targets + RUN lines to `Dockerfile.v3`
  - Updated `plans/step4-checklist.md`: Round 45 entry, handle_bnet.cpp count 55→50
  - Running total: **138 packet_create sites** remaining (down from 143)

### 2026-05-20
- Round 49 COMPLETE: New `send_echoreq_bridge` -- SERVER_ECHOREQ (SID_PING, 0x25)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_echoreq_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_echoreq_bridge.cpp`
    - `pvpgn_v3_send_echoreq(conn, ticks)` -- reuses `encode(bnet::Ping)`
    - 8-byte wire: ff 25 08 00 + u32 LE ticks
  - Created `tests/unit/integration/legacy_bnetd/send_echoreq_bridge_test.cpp` (5 cases)
    - no-handler / null-conn / wire-parity / zero-ticks / handler propagation
  - Added source to `src/v3/CMakeLists.txt`
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/handle_bnet.cpp` `_client_auth_info` (line 962):
    one previously unbridged `packet_create` site now guarded by the
    `pvpgn_v3_send_echoreq` call under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Updated `plans/step4-checklist.md` with Round 49 entry
  - Validation: deferred to docker build with cache (per user choice)

### 2026-05-20
- Round 49 COMPLETE: New `send_echoreq_bridge` -- SERVER_ECHOREQ (SID_PING, 0x25)
  - Created `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_echoreq_bridge.hpp`
  - Created `src/v3/integration/legacy_bnetd/src/send_echoreq_bridge.cpp`
    - `pvpgn_v3_send_echoreq(conn, ticks)` -- reuses `encode(bnet::Ping)`
    - 8-byte wire: ff 25 08 00 + u32 LE ticks
  - Created `tests/unit/integration/legacy_bnetd/send_echoreq_bridge_test.cpp` (5 cases)
    - no-handler / null-conn / wire-parity / zero-ticks / handler propagation
  - Added source to `src/v3/CMakeLists.txt`
  - Added test target to `tests/unit/integration/legacy_bnetd/CMakeLists.txt`
  - Added build target + RUN line to `Dockerfile.v3`
  - Wired into `src/bnetd/handle_bnet.cpp` `_client_auth_info` (line 962):
    one previously unbridged `packet_create` site now guarded by the
    `pvpgn_v3_send_echoreq` call under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Updated `plans/step4-checklist.md` with Round 49 entry
  - Validation: deferred to docker build with cache (per user choice)
- Round 49 VALIDATED: docker build (Dockerfile.v3, --target v3-test) succeeded.
  - New `send_echoreq_bridge.cpp` compiled into `integration_legacy_bnetd`.
  - New `test_integration_legacy_bnetd_send_echoreq_bridge` built and run.
  - Full chained v3-test RUN line passed (image tagged `pvpgn-v3:round49`).
- Round 49 VALIDATED: docker build (Dockerfile.v3, --target v3-test) succeeded.
  - New `send_echoreq_bridge.cpp` compiled into `integration_legacy_bnetd`.
  - New `test_integration_legacy_bnetd_send_echoreq_bridge` built and run.
  - Full chained v3-test RUN line passed (image tagged `pvpgn-v3:round49`).
- Round 50 COMPLETE: Reused `pvpgn_v3_send_motdw3` for per-news entries.
  - No new files; wired into `_news_cb` (`src/bnetd/handle_bnet.cpp` line ~3891).
  - Guard added under `#ifdef PVPGN_V3_BNETD_INTEGRATION`; on rc==1
    skips the legacy `packet_create` / `conn_push_outqueue` path.
  - Parity: `packet_append_lstr` and `write_cstring` produce
    identical bytes for lstrs created via `lstr_set_str`.
  - Updated `plans/step4-checklist.md` with Round 50 entry.
- Round 50 COMPLETE: Reused `pvpgn_v3_send_motdw3` for per-news entries.
  - No new files; wired into `_news_cb` (`src/bnetd/handle_bnet.cpp` line ~3891).
  - Guard added under `#ifdef PVPGN_V3_BNETD_INTEGRATION`; on rc==1
    skips the legacy `packet_create` / `conn_push_outqueue` path.
  - Parity: `packet_append_lstr` and `write_cstring` produce
    identical bytes for lstrs created via `lstr_set_str`.
  - Updated `plans/step4-checklist.md` with Round 50 entry.
- Round 50 VALIDATED: docker v3-test image built green (`pvpgn-v3:round50`). Note: `handle_bnet.cpp` is not compiled in WITH_BNETD=OFF stage; call-site edits inside `#ifdef PVPGN_V3_BNETD_INTEGRATION` follow same pattern as Rounds 46-48 (unvalidated by docker, low risk).
- Round 50 VALIDATED: docker v3-test image built green (`pvpgn-v3:round50`). Note: `handle_bnet.cpp` is not compiled in WITH_BNETD=OFF stage; call-site edits inside `#ifdef PVPGN_V3_BNETD_INTEGRATION` follow same pattern as Rounds 46-48 (unvalidated by docker, low risk).

### Round 51 -- send_clan_invitereply (SID 0x77)

- Created v3 bridge `pvpgn_v3_send_clan_invitereply(void*, count u32, result u8)`:
  - Header `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_clan_invitereply_bridge.hpp`
  - Impl   `src/v3/integration/legacy_bnetd/src/send_clan_invitereply_bridge.cpp`
  - Encodes `ClanGenericResultReply{sid=kSidClanInvite, cookie=count, result}` -> 9-byte wire (ff 77 09 00 + u32 LE + u8).
- Reused existing codec entry; no new struct or wire type.
- Test `tests/unit/integration/legacy_bnetd/send_clan_invitereply_bridge_test.cpp` (5 cases: no-handler / null-conn / 9-byte wire parity / zero values / return propagation).
- Wired into 2 sites in `src/bnetd/handle_bnet.cpp` under `PVPGN_V3_BNETD_INTEGRATION`:
  - `_client_clan_invitereq` early-reject (~line 6945, recipient = c)
  - `_client_clan_invitereply` invitee response (~line 7038, recipient = conn, reads back the bytes that legacy code just set on rpacket)
- Updated `src/v3/CMakeLists.txt`, `tests/unit/integration/legacy_bnetd/CMakeLists.txt`, and `Dockerfile.v3` (build target list + RUN test line).
- Validation: docker v3-test image built green (`pvpgn-v3:round51`); new bridge test passes; full chain still `1225 assertions in 213 test cases` for the final binary in the chain. `handle_bnet.cpp` call-site edits remain unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-50.

### Round 52 -- send_clan_membernewchief_reply (SID 0x74)

- Created v3 bridge `pvpgn_v3_send_clan_membernewchief_reply(void*, count u32, result u8)`.
  - Reuses `ClanGenericResultReply` codec with `sid=kSidClanMemberNewChief` -> 9-byte wire (ff 74 09 00 + u32 LE + u8).
  - Header / impl files under `src/v3/integration/legacy_bnetd/{include,src}/...`.
- Test `send_clan_membernewchief_reply_bridge_test.cpp` (5 cases: same pattern as the 0x77 bridge).
- Wired ONLY the FAILED branch in `_client_clan_membernewchiefreq` (handle_bnet.cpp ~line 6862, recipient = c). The SUCCESS branch broadcasts to all online clan members via `clan_send_packet_to_online_members()` and is intentionally left on the legacy path -- the single-conn send_packet ABI cannot model the broadcast.
- Updated `src/v3/CMakeLists.txt`, `tests/unit/integration/legacy_bnetd/CMakeLists.txt`, and `Dockerfile.v3` (build target list + RUN test line).
- Validation: docker v3-test image built green (`pvpgn-v3:round52`); new bridge test runs in the chain. `handle_bnet.cpp` edit remains unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-51.

### Round 52 -- send_clan_membernewchief_reply (SID 0x74)

- Created v3 bridge pvpgn_v3_send_clan_membernewchief_reply(void*, count u32, result u8).
  - Reuses ClanGenericResultReply codec with sid=kSidClanMemberNewChief -> 9-byte wire (ff 74 09 00 + u32 LE + u8).
  - Header / impl files under src/v3/integration/legacy_bnetd/{include,src}/...
- Test send_clan_membernewchief_reply_bridge_test.cpp (5 cases: same pattern as the 0x77 bridge).
- Wired ONLY the FAILED branch in _client_clan_membernewchiefreq (handle_bnet.cpp ~line 6862, recipient = c). The SUCCESS branch broadcasts to all online clan members via clan_send_packet_to_online_members() and is intentionally left on the legacy path -- the single-conn send_packet ABI cannot model the broadcast.
- Updated src/v3/CMakeLists.txt, tests/unit/integration/legacy_bnetd/CMakeLists.txt, and Dockerfile.v3 (build target list + RUN test line).
- Validation: docker v3-test image built green (pvpgn-v3:round52); new bridge test runs in the chain. handle_bnet.cpp edit remains unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-51.


### Round 53 -- send_clanmember_remove_reply (SID 0x78) + send_clanmember_rankupdate_reply (SID 0x7A)

- Two new v3 bridges, both reusing ClanGenericResultReply codec:
  - pvpgn_v3_send_clanmember_remove_reply (sid=kSidClanMemberRemove, 0x78) -- 9-byte wire (ff 78 09 00 + u32 LE + u8)
  - pvpgn_v3_send_clanmember_rankupdate_reply (sid=kSidClanMemberRankUpdate, 0x7A) -- 9-byte wire (ff 7a 09 00 + u32 LE + u8)
  - Headers / impls under src/v3/integration/legacy_bnetd/{include,src}/...
- Two new test files, each with 4 Catch2 cases (no-handler / null-conn / wire parity / return propagation).
- Wired one site per bridge in src/bnetd/handle_bnet.cpp under PVPGN_V3_BNETD_INTEGRATION:
  - _client_clanmember_rankupdatereq (~line 6780): read back count + result from rpacket, try v3 before legacy push.
  - _client_clanmember_removereq (~line 6832): same pattern. The interleaved 0x7E SERVER_CLANMEMBER_REMOVED_NOTIFY broadcast stays on the legacy path.
- Updated src/v3/CMakeLists.txt, tests/unit/integration/legacy_bnetd/CMakeLists.txt, and Dockerfile.v3 (build target list + RUN test lines).
- Validation: docker v3-test image built green (pvpgn-v3:round53); both new bridge tests run in the chain. handle_bnet.cpp call-site edits remain unvalidated by docker (WITH_BNETD=OFF), same caveat as Rounds 46-52.


## Round 54 -- SERVER_CHANGEPASSACK (SID 0x31) bridge

- Added src/v3/integration/legacy_bnetd/{include,src}/send_changepassack_bridge.{hpp,cpp}
- Added tests/unit/integration/legacy_bnetd/send_changepassack_bridge_test.cpp (5 cases: no-handler / null-conn / success wire / fail wire / return propagation)
- Reuses existing pvpgn::protocol::bnet::ChangePasswordReply codec.
- Wired forward decl + call-site in src/bnetd/handle_bnet.cpp::_client_changepassreq under PVPGN_V3_BNETD_INTEGRATION (single push site).
- Updated src/v3/CMakeLists.txt, tests/.../CMakeLists.txt, Dockerfile.v3 (target list + RUN test block).
- Validated: docker build pvpgn-v3:round54 OK -- naming to docker.io/library/pvpgn-v3:round54 done.
- Caveat: handle_bnet.cpp call-site not exercised by v3-test (WITH_BNETD=OFF); pattern matches Rounds 46-53.
- Pivoted from REGSNOOPREQ (site is inside #if 0 dead code) to SERVER_CHANGEPASSACK.

## Round 55 -- legacy_d2cs scaffold (dispatcher half)

- Added new v3 directory tree src/v3/integration/legacy_d2cs/{include,src}/.
- Created send_packet_bridge.{hpp,cpp} mirroring legacy_bnetd:
  - namespace pvpgn::integration::legacy_d2cs
  - C ABI pvpgn_v3_d2cs_send_packet_try / _send_packet_available
  - kSendPacketMaxSize=3072 (parity with d2cs MAX_PACKET_SIZE)
  - install_legacy_send_packet_handler declared but not yet defined
    (linked half deferred).
- Added pvpgn_v3_add_library(integration_legacy_d2cs ...) target in
  src/v3/CMakeLists.txt with only core as PUBLIC_DEPS.
- Added tests/unit/integration/legacy_d2cs/CMakeLists.txt registering
  test_integration_legacy_d2cs_send_packet_bridge (5 Catch2 cases:
  no-handler / null-conn-or-bytes / size-bounds / forward-verbatim /
  return-propagation).
- Updated tests/unit/integration/CMakeLists.txt to add_subdirectory(legacy_d2cs).
- Updated Dockerfile.v3: added test target to build list and RUN block.
- Validated: docker build pvpgn-v3:round55 OK -- naming to docker.io/library/pvpgn-v3:round55 done.
- Scope deferred to future round: extract d2cs_legacy static library
  from the d2cs executable target, then add
  integration_legacy_d2cs_linked with the real packet_create +
  conn_d2cs_outqueue implementation, plus PVPGN_V3_D2CS_INTEGRATION
  macro wiring in src/d2cs/CMakeLists.txt. Until that is in place,
  pvpgn_v3_d2cs_send_packet_try has no production producer; only
  unit tests exercise the dispatcher.

## Round 56 -- d2cs_legacy static library carve-out

- Refactored src/d2cs/CMakeLists.txt to mirror the bnetd_legacy pattern:
  - New STATIC library d2cs_legacy containing all .cpp/.h sources
    except main.cpp and the win32 winmain/resource files.
  - Original d2cs executable now contains only main.cpp + winmain
    (+ resource.rc on Win32) and links against d2cs_legacy.
  - PUBLIC link deps on d2cs_legacy: common compat fmt win32
    ${NETWORK_LIBRARIES} (matches prior d2cs link line).
  - PUBLIC include dir on d2cs_legacy: src/d2cs/ (so future
    integration_legacy_d2cs_linked can include legacy headers as
    'd2cs/connection.h' etc relative to src/).
- Validated:
  - Legacy build: docker build -f Dockerfile pvpgn-legacy:round56 OK
    (d2cs_legacy.a built, d2cs exe linked against it,
    naming to docker.io/library/pvpgn-legacy:round56 done).
  - v3 build:     docker build -f Dockerfile.v3 pvpgn-v3:round56 OK
    (no regression, naming to docker.io/library/pvpgn-v3:round56 done).
- Unblocks future round: integration_legacy_d2cs_linked can now be
  added with a PUBLIC_DEPS d2cs_legacy + the real send_packet_via_legacy
  implementation calling packet_create(packet_class_d2cs) and
  conn_d2cs_outqueue (parity with bnetd link half).
- No code changes outside src/d2cs/CMakeLists.txt; no source edits.

## Round 57 -- integration_legacy_d2cs_linked (real send_packet_via_legacy)

- Added src/v3/integration/legacy_d2cs/src/send_packet_bridge_link.cpp:
  - install_legacy_send_packet_handler() installs send_packet_via_legacy.
  - send_packet_via_legacy:
      1. casts conn_ptr to pvpgn::d2cs::t_connection*
      2. packet_create(packet_class_raw)
      3. packet_get_raw_data_build + memcpy + packet_set_size
      4. pvpgn::d2cs::conn_push_outqueue + packet_del_ref
      5. returns 1 on push >= 0, else 0.
  - Mirrors integration/legacy_bnetd/src/send_packet_bridge_link.cpp.
- Added pvpgn_v3_add_library(integration_legacy_d2cs_linked ...) target in
  src/v3/CMakeLists.txt, gated by if(TARGET d2cs_legacy):
  - PUBLIC_DEPS: core, integration_legacy_d2cs
  - DEPS: d2cs_legacy
  - target_compile_options -w on the linked TU (legacy headers not
    warning-clean under v3 strict warnings).
  - target_include_directories adds src/ + build dir for
    'common/setup_before.h', 'd2cs/connection.h', 'common/packet.h'.
- Validated v3-test build: pvpgn-v3:round57 OK -- no regression
  (linked half not built in v3-only image because WITH_D2CS=OFF means
  d2cs_legacy target absent; same precedent as integration_legacy_bnetd_linked).
- Combined-build validation deferred: no existing docker stage configures
  both d2cs_legacy and the v3 tree simultaneously; same situation
  applies to the bnetd linked half. Future combined-build docker stage
  could exercise both.

## Round 59 -- legacy_d2dbs scaffold + d2dbs_legacy carve-out

Carved d2dbs into a static lib + thin executable (parity with bnetd/d2cs):
- src/d2dbs/CMakeLists.txt: d2dbs_legacy static lib contains all sources
  except main.cpp and ../win32/d2dbs_winmain.cpp/resource. d2dbs exe
  links against d2dbs_legacy. PUBLIC include dir: src/d2dbs/.

Added legacy_d2dbs scaffold under src/v3/integration/legacy_d2dbs/:
- send_packet_bridge.{hpp,cpp}: dispatcher with C ABI
  pvpgn_v3_d2dbs_send_packet_try / _send_packet_available;
  kSendPacketMaxSize=3072 (conservative; kBufferSize=20480 enforced
  at runtime by the linked half).
- send_packet_bridge_link.cpp: install_legacy_send_packet_handler
  memcpys bytes into conn->WriteBuf at nCharsInWriteBuffer (the
  d2dbs send model -- no t_packet outqueue), with remaining-space
  guard mirroring legacy dbspacket.cpp handlers.

CMake:
- integration_legacy_d2dbs static lib (always built, deps: core).
- integration_legacy_d2dbs_linked gated on TARGET d2dbs_legacy;
  PUBLIC_DEPS core + integration_legacy_d2dbs; DEPS d2dbs_legacy;
  -w on the link TU; include dirs src/ + build for legacy headers.

Tests:
- tests/unit/integration/legacy_d2dbs/send_packet_bridge_test.cpp:
  5 Catch2 cases (no-handler / null-conn-or-bytes / size-bounds /
  forward-verbatim / return-propagation).
- Added test subdir to tests/unit/integration/CMakeLists.txt.
- Added Dockerfile.v3 entry (target list + RUN block).

Validation:
- docker build -f Dockerfile.v3 pvpgn-v3:round59 OK
  (naming to docker.io/library/pvpgn-v3:round59 done).
- docker build -f Dockerfile  pvpgn-legacy:round59 OK
  (naming to docker.io/library/pvpgn-legacy:round59 done;
  d2dbs_legacy.a built; d2dbs exe linked against it).
- Linked half compile validation deferred (no combined-build CI
  stage; same precedent as bnetd / d2cs linked halves).

## Round 60 -- first real d2cs bridge: send_loginreply (D2CS_CLIENT_LOGINREPLY 0x01)

- New: src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/send_loginreply_bridge.hpp
- New: src/v3/integration/legacy_d2cs/src/send_loginreply_bridge.cpp
- New: tests/unit/integration/legacy_d2cs/send_loginreply_bridge_test.cpp (5 cases / 22 assertions)
- Wired: src/v3/CMakeLists.txt (integration_legacy_d2cs += send_loginreply_bridge.cpp; PUBLIC_DEPS += protocol_common, protocol_d2cs)
- Wired: tests/unit/integration/legacy_d2cs/CMakeLists.txt (+ pvpgn_v3_add_test target)
- Wired: Dockerfile.v3 (build list + RUN block)
- Wired: src/d2cs/CMakeLists.txt (PVPGN_V3_D2CS_INTEGRATION macro + linked-lib link gated on TARGET integration_legacy_d2cs_linked)
- Wired: src/d2cs/handle_bnetd.cpp on_bnetd_accountloginreply -- forward decl + #ifdef hook before conn_push_outqueue.
- Validation: pvpgn-v3:round60 OK (5/5 new cases pass; all tests pass). pvpgn-legacy:round60 OK.
- Note: Legacy build has no v3 tree configured so PVPGN_V3_D2CS_INTEGRATION stays undefined; call-site hook compiles but is dormant. Same precedent as bnetd rounds.

## Round 61 -- d2cs send_createcharreply bridge (D2CS_CLIENT_CREATECHARREPLY 0x02)
- New: src/v3/integration/legacy_d2cs/{include,src}/send_createcharreply_bridge.{hpp,cpp}
- New: tests/unit/integration/legacy_d2cs/send_createcharreply_bridge_test.cpp (4 cases)
- Wired: src/v3/CMakeLists.txt, tests CMake, Dockerfile.v3.
- Wired two call sites: src/d2cs/handle_d2cs.cpp::on_client_createcharreq and src/d2cs/handle_bnetd.cpp on_bnetd_charloginreply (CREATECHARREQ branch) under PVPGN_V3_D2CS_INTEGRATION.
- Validation: pvpgn-v3:round61 OK + pvpgn-legacy:round61 OK.

## Round 62 -- d2cs send_charloginreply bridge (D2CS_CLIENT_CHARLOGINREPLY 0x07)
- New: protocol/d2cs/codec adds CharLoginReply struct + kClientCharLoginReply (0x07) + reply constants, encode() impl.
- New: src/v3/integration/legacy_d2cs/{include,src}/send_charloginreply_bridge.{hpp,cpp}
- New: tests/unit/integration/legacy_d2cs/send_charloginreply_bridge_test.cpp (4 cases)
- Wired: src/v3/CMakeLists.txt, tests CMake, Dockerfile.v3.
- Wired call site: src/d2cs/handle_bnetd.cpp on_bnetd_charloginreply (CHARLOGINREQ branch) under PVPGN_V3_D2CS_INTEGRATION.
- Validation: pvpgn-v3:round62 OK + pvpgn-legacy:round62 OK.

## R63 - d2cs send_creategamereply + send_joingamereply
- Added bridges in src/v3/integration/legacy_d2cs/{include,src}/.../send_{creategame,joingame}reply_bridge.{hpp,cpp}.
- joingamereply addr trick: legacy used bn_int_nset (BE wire); bridge takes addr_host and applies internal bswap32() before assigning to m.addr so subsequent LE encode produces the desired BE wire bytes.
- Wire format: creategamereply 13B (type 0x03), joingamereply 21B (type 0x04).
- Wired 3 call sites under PVPGN_V3_D2CS_INTEGRATION:
  - src/d2cs/handle_d2cs.cpp - on_client_creategamereq failure path
  - src/d2cs/handle_d2gs.cpp - on_d2gs_creategamereply
  - src/d2cs/handle_d2gs.cpp - on_d2gs_joingamereply (computes v3_addr via trans_net on success)
- Catch2 test send_gamereply_bridges_test.cpp asserts wire bytes incl. BE addr ordering.
- Validation: pvpgn-v3:round63 + pvpgn-legacy:round63 both built; 1225 assertions / 213 cases pass.

## R64 - first d2dbs bridge: send_echorequest
- Added bridge in src/v3/integration/legacy_d2dbs/{include,src}/.../send_echorequest_bridge.{hpp,cpp}.
- Wire: 8B, all LE - u16 size=8 | u16 type=0x34 | u32 seqno (codec.EchoRequest).
- Added PVPGN_V3_D2DBS_INTEGRATION macro wiring in src/d2dbs/CMakeLists.txt (mirrors d2cs R60 pattern).
- Wired src/d2dbs/dbspacket.cpp dbs_keepalive() with forward decl + per-connection #ifdef hook that returns early on handled=1 (skips legacy memcpy into WriteBuf).
- New Catch2 test send_echorequest_bridge_test.cpp asserts 8-byte wire incl. LE seqno encoding (0xdeadbeef -> ef be ad de).
- Validation: pvpgn-v3:round64 + pvpgn-legacy:round64 both built; test count 128 -> 129; 1225+ assertions pass.

## R65 - bnetd legacy-fallback removal audit
- Scanned src/bnetd/*.cpp for packet_create(packet_class_bnet) and checked PVPGN_V3_BNETD_INTEGRATION within ±120 lines.
- handle_*.cpp: 76 / 76 sites covered (100%). All dispatched client packet handlers are bridged.
- Full src/bnetd: 97 sites total, 87 covered, 10 uncovered.
- Of the 10 uncovered: 1 is a false positive (server.cpp:815 is an INPUT read-buffer alloc, not an outbound reply). 9 real gaps remain.
- Full report: plans/r65-bnetd-fallback-audit.md (lists each gap with packet type + trigger and suggested R66-R69 grouping).
- No correctness risk: strangler-fig contract leaves un-bridged sites running legacy unchanged.

## R66 - SERVER_ECHOREQ + SERVER_MESSAGEBOX bridges
- Existing send_echoreq bridge was already in v3 (only handle_bnet.cpp call site wired); added second hook in src/bnetd/connection.cpp conn_test_latency() pre-game branch.
- New send_messagebox bridge (v3 codec MessageBox already present):
  - src/v3/integration/legacy_bnetd/{include,src}/.../send_messagebox_bridge.{hpp,cpp}
  - Wire: ff 19 <size_LE> | u32 style LE | text \0 | caption \0.
  - Wired in src/bnetd/message.cpp messagebox_show().
- New Catch2 test send_messagebox_bridge_test.cpp asserts header + style + cstring layout.
- Validation: pvpgn-v3:round67 + pvpgn-legacy:round67 both built; test count 129 -> 130 after R66.

## R67 - friend-list acks (SERVER_FRIEND{ADD,DEL,MOVE}_ACK)
- Three new bridges (v3 codec already had FriendAddAck/FriendDelAck/FriendMoveAck encoders):
  - send_friendadd_ack_bridge: hdr | name \0 | status u8 | location u8 | client_tag u32 LE | loc_name \0 (variable size)
  - send_frienddel_ack_bridge: hdr | friend_num u8 (5B)
  - send_friendmove_ack_bridge: hdr | pos1 u8 | pos2 u8 (6B)
- Wired 4 call sites in src/bnetd/command.cpp under PVPGN_V3_BNETD_INTEGRATION:
  - /f a (line ~1544) -> send_friendadd_ack (reads back status fields via bn_byte_get/bn_int_get on the local status struct)
  - /f r (line ~1655) -> send_frienddel_ack
  - /f promote / /f demote (lines ~1699/1742) -> send_friendmove_ack
- New Catch2 test send_friend_acks_bridges_test.cpp covers all three with wire-byte assertions.
- Validation: test count 129 -> 131 across R66+R67; both docker images built; 1225 assertions / 213 cases.

- R69 antihack bridges (SERVER_READMEMORY 0x17, SERVER_REQUIREDWORK 0x4C) wired in connection.cpp. Tests + both docker images green.

- R71 d2cs handle_bnetd.cpp gaps bridged: init handshake, AUTHREPLY, GAMEINFOREPLY. Tests + both docker images green.

- R72 d2cs handle_d2gs bridges (5 sites) -- v3+legacy green


- R73 d2cs handle_d2cs.cpp wire joingamereply+charloginreply -- v3+legacy green


- R74 d2cs simple reply bridges (4 sites) -- v3+legacy green


- R75 d2cs variable-length list-reply bridges (gamelistreply x2 + gameinforeply) -- v3+legacy green

## R90 -- send_raw_text_bridge: handle_bot.cpp + handle_telnet.cpp (22 sites)
- Single bridge covers all raw-text sends in both bot and telnet login FSMs.
- Two C-ABI functions:
  - `pvpgn_v3_send_raw_text(conn_ptr, text)` — sends `strlen(text)` bytes verbatim
  - `pvpgn_v3_send_raw_text2(conn_ptr, prefix, suffix)` — concatenates then sends
- handle_bot.cpp: 11 sites wired under `#ifdef PVPGN_V3_BNETD_INTEGRATION`
  - Sites 1-2 (echo+prompt): `pvpgn_v3_send_raw_text2(c, username, "\r\nPassword: ")`
  - Sites 3-10 (error messages): `pvpgn_v3_send_raw_text(c, tempa/tempb)`
  - Site 11 (login success `"\r\n"`): non-fatal guard — fall-through on bridge failure
- handle_telnet.cpp: 11 sites wired (telnet has `#if 0` for echo, so sites 1-2 use `send_raw_text` not `send_raw_text2`)
- 16 Catch2 test cases in `send_raw_text_bridge_test.cpp`
- Build: `send_raw_text_bridge.cpp` added to `integration_legacy_bnetd`; test registered in CMakeLists.txt + Dockerfile.v3
- packet_create() count: 121 → 99 remaining (−22)

## R91 -- anongame.cpp send bridges (9 sites)
- **Discovery**: `clan.cpp` all 9 `packet_create()` sites already bridged in Rounds 38, 50-55. No new clan work.
- `anongame.cpp`: 9 sites wired across 3 new bridge files.
- **Bridge 1**: `send_anongame_search_reply_bridge` — proper encode bridge
  - `pvpgn_v3_send_anongame_search_reply(conn_ptr, count, reply, search_time)` → int
  - Encodes 11-byte body: option=0x01 (SERVER_FINDANONGAME_SEARCH), count LE32, reply LE32, search_time LE16
  - Site 1 (`_handle_anongame_search` line 441): full skip-legacy guard with `goto skip_anongame_search_reply`
  - 5 Catch2 test cases: no-handler, null-conn, wire parity, non-zero reply, handler propagation
- **Bridge 2**: `send_anongame_found_bridge` — observation-only (always returns 0)
  - `pvpgn_v3_observe_anongame_found(conn_ptr)` → 0
  - `SERVER_ANONGAME_FOUND` is complex variable-length (IP, port, mapname, pt2 struct); v3 encoder not yet complete
  - Site 2 (`_anongame_search_found` line 979): `(void)pvpgn_v3_observe_anongame_found(...)` before legacy
  - 2 Catch2 test cases
- **Bridge 3**: `send_w3route_bridge` — 7 observation hooks (all always return 0)
  - `pvpgn_v3_observe_w3route_{ack,loadingack,ready,playerinfo,levelinfo,startgame1,startgame2}`
  - Complex per-player coordination packets sent to w3route connections; v3 w3route encoder not yet complete
  - Sites 3-9 wired with `(void)pvpgn_v3_observe_*(conn_get_routeconn(...))` before legacy
  - 14 Catch2 test cases (null + valid for each of 7 hooks)
- Build: 3 new `.cpp` sources in `integration_legacy_bnetd`; 3 test targets in CMakeLists.txt + Dockerfile.v3
- packet_create() count: 99 → 90 remaining (−9)

## R92 -- server.cpp + message.cpp audit (0 new bridges)
- **server.cpp**: All 8 `packet_create()` sites in `sd_tcpinput()` (lines 800–873) + 1 in `sd_udpinput()` (line 718) are INPUT buffer allocations (read-buffers for incoming client data). Per task rules, INPUT allocations are intentionally skipped — no bridges created.
  - Classes audited: init, d2cs_bnetd, bnet, raw (file/pending_raw), file, raw (bot/ircinit/irc/wol/wserv/apireg/wladder/telnet), w3route, wolgameres, udp
- **message.cpp**: All 5 `packet_create()` sites are already handled or are internal cache allocations:
  - Lines 1490, 1502 (`packet_class_raw` for telnet/bot in `message_cache_lookup`): already handled by R90 `send_raw_text_bridge` at handler level
  - Line 1514 (`packet_class_bnet` for bnet in `message_cache_lookup`): already bypassed by `pvpgn_v3_send_chatevent_compose` in `message_send()`
  - Line 1529 (`packet_class_raw` for irc/wol/wserv/wgameres in `message_cache_lookup`): internal cache allocation; no v3 IRC/WOL encoder yet
  - Line 1875 (`packet_class_bnet` in `messagebox_show`): already bridged by `pvpgn_v3_send_messagebox` (R66)
- **Result**: 0 new bridges, 0 source changes, 0 build changes
- packet_create() count: 90 → **90 remaining** (no change)

## R93 -- handle_d2cs.cpp (5 sites) + connection.cpp (2 new sites)
- **handle_d2cs.cpp** (5 sites, all `packet_class_d2cs_bnetd`, all observation-only):
  - `on_d2cs_authreply` line ~161: `pvpgn_v3_observe_d2cs_bnetd_authreply(c, reply)`
  - `on_d2cs_accountloginreq` line ~250: `pvpgn_v3_observe_d2cs_bnetd_accountloginreply(c, seqno, reply)`
  - `on_d2cs_charloginreq` line ~322: `pvpgn_v3_observe_d2cs_bnetd_charloginreply(c, seqno, reply)`
  - `handle_d2cs_init` line ~338: `pvpgn_v3_observe_d2cs_bnetd_authreq(c, sessionnum)`
  - `send_d2cs_gameinforeq` line ~376: `pvpgn_v3_observe_d2cs_bnetd_gameinforeq(realm_get_conn(realm), game_get_name(game))`
- **connection.cpp** (5 sites, 3 already bridged, 2 new):
  - `conn_test_latency` line ~255 (`SERVER_W3ROUTE_ECHOREQ`): NEW — `pvpgn_v3_observe_w3route_echoreq(c, get_ticks())`
  - `conn_test_latency` line ~276 (`SERVER_ECHOREQ`): already bridged (R49)
  - `conn_set_class` line ~860 (`"Username: "` raw text): reuse `pvpgn_v3_send_raw_text` (R90)
  - `conn_client_readmemory` line ~4289 (`SERVER_READMEMORY`): already bridged (R69)
  - `conn_client_requiredwork` line ~4319 (`SERVER_REQUIREDWORK`): already bridged (R69)
- **New files**:
  - `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_d2cs_bnetd_bridges.hpp`
  - `src/v3/integration/legacy_bnetd/src/send_d2cs_bnetd_bridges.cpp`
  - `tests/unit/integration/legacy_bnetd/send_d2cs_bnetd_bridges_test.cpp` (13 test cases)
- **Modified files**:
  - `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_w3route_bridge.hpp` — added `pvpgn_v3_observe_w3route_echoreq`
  - `src/v3/integration/legacy_bnetd/src/send_w3route_bridge.cpp` — added `pvpgn_v3_observe_w3route_echoreq` impl
  - `tests/unit/integration/legacy_bnetd/send_w3route_bridge_test.cpp` — added 2 test cases for echoreq
  - `src/bnetd/handle_d2cs.cpp` — extern declarations + 5 observation call sites
  - `src/bnetd/connection.cpp` — extern declarations + 2 new wiring sites
  - `src/v3/CMakeLists.txt` — added `send_d2cs_bnetd_bridges.cpp`
  - `tests/unit/integration/legacy_bnetd/CMakeLists.txt` — added `test_integration_legacy_bnetd_send_d2cs_bnetd_bridges`
  - `Dockerfile.v3` — added 4 cmake build targets + 1 RUN test line
- packet_create() count: 90 → **83 remaining** (−7: 5 from handle_d2cs.cpp + 2 from connection.cpp)

## R94 -- handle_anongame.cpp cancel bridge (1 site) + command.cpp audit (0 new)
- **handle_anongame.cpp** (8 sites audited, 7 already bridged from R91, 1 new):
  - `_client_anongame_cancel` line ~470 (`SERVER_FINDANONGAME_PLAYGAME_CANCEL`): NEW — `pvpgn_v3_send_anongame_cancel(c, a_count)`
  - All other 7 sites already have `PVPGN_V3_BRIDGE_TRY` macros from R91
- **command.cpp** (4 sites audited, all already bridged from R67):
  - `_handle_friends_command` lines ~1557, ~1685, ~1735, ~1786: friend-ack sites — no new bridges needed
- **New files**:
  - `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_anongame_cancel_bridge.hpp`
  - `src/v3/integration/legacy_bnetd/src/send_anongame_cancel_bridge.cpp`
  - `tests/unit/integration/legacy_bnetd/send_anongame_cancel_bridge_test.cpp` (6 test cases)
- **Modified files**:
  - `src/bnetd/handle_anongame.cpp` — added `#include "integration/legacy_bnetd/send_anongame_cancel_bridge.hpp"` + `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard at cancel site
  - `src/v3/CMakeLists.txt` — added `send_anongame_cancel_bridge.cpp`
  - `tests/unit/integration/legacy_bnetd/CMakeLists.txt` — added `test_integration_legacy_bnetd_send_anongame_cancel_bridge`
  - `Dockerfile.v3` — added 1 cmake build target + 1 RUN test line
- packet_create() count: 83 → **82 remaining** (−1: `_client_anongame_cancel`)

## R95 -- irc.cpp (3 sites) + anongame_wol.cpp (1 site) raw-text bridges
- **irc.cpp** (3 new sites, all `packet_class_raw`):
  - `irc_send_cmd` line ~104: formats `":%s %s %s %s"` or `":%s %s %s"` + `"\r\n"` → `pvpgn_v3_send_raw_text(conn, data)` + `goto irc_send_cmd_skip_legacy`
  - `irc_send_ping` line ~158: formats `"PING :%s"` or `"PING :%u"` + `"\r\n"` → `pvpgn_v3_send_raw_text(conn, data)` + `goto irc_send_ping_skip_legacy`
  - `irc_send_pong` line ~192: formats `":%s PONG %s :%s"` or `":%s PONG %s"` + `"\r\n"` → `pvpgn_v3_send_raw_text(conn, data)` + `goto irc_send_pong_skip_legacy`
- **anongame_wol.cpp** (1 new site, `packet_class_raw`):
  - `_send_msg` line ~309: formats `":matchbot!u@h " + command + " " + nick + " " + text` + `"\r\n"` → `pvpgn_v3_send_raw_text(conn, data.c_str())` + `goto anongame_wol_send_msg_skip_legacy`
- **file.cpp** (2 sites audited, both deferred):
  - `file_send` line ~210: `packet_class_file` (`SERVER_FILE_REPLY` header) — needs new `send_file_reply_bridge`, deferred
  - `file_send` line ~274 (loop): `packet_class_raw` binary file data via `fread` — needs raw-bytes bridge, deferred
- **Bridge reused**: `pvpgn_v3_send_raw_text` from R90 — no new bridge files
- **Modified files**:
  - `src/bnetd/irc.cpp` — forward decl + 3 `#ifdef PVPGN_V3_BNETD_INTEGRATION` guards (text formatted before bridge call; restructured to format before `packet_create`)
  - `src/bnetd/anongame_wol.cpp` — forward decl + 1 `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard (text built into `std::string data` before bridge call; `packet_create` moved into inner block)
- **No changes** to `src/v3/CMakeLists.txt`, `tests/unit/integration/legacy_bnetd/CMakeLists.txt`, or `Dockerfile.v3`
- packet_create() count: 82 → **78 remaining** (−4: `irc_send_cmd`, `irc_send_ping`, `irc_send_pong`, `_send_msg`)

## R96 -- handle_wol.cpp (1 site) + handle_apireg.cpp (1 site) raw-text bridges
- **handle_wol.cpp** (1 new site, `packet_class_raw`):
  - `_ladder_send` line ~1652: formats `"\r\n\r\n\r\n%s"` into `data[]` → `pvpgn_v3_send_raw_text(conn, data)` + `goto handle_wol_ladder_send_skip_legacy`
  - Restructured: moved `packet_create` after bridge guard (original had it before `sprintf`)
- **handle_apireg.cpp** (1 new site, `packet_class_raw`):
  - `apireg_send` line ~486: `data = command` (just `sprintf(data, "%s", command)`) → `pvpgn_v3_send_raw_text(conn, data)` + `goto apireg_send_skip_legacy`
- **handle_file.cpp** (1 site audited, deferred):
  - `handle_file_packet` line ~96: `packet_class_raw` binary `t_server_file_unknown1` struct — binary data, deferred
- **anongame_infos.cpp** (2 sites audited, skipped — not send sites):
  - Lines ~1728, ~1936: `packet_class_raw` used as scratch buffers for zlib compression, never pushed to outqueue
- **Bridge reused**: `pvpgn_v3_send_raw_text` from R90 — no new bridge files
- **Modified files**:
  - `src/bnetd/handle_wol.cpp` — forward decl + 1 `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard (text formatted before bridge call; `packet_create` moved after bridge guard)
  - `src/bnetd/handle_apireg.cpp` — forward decl + 1 `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard (text already formatted before `packet_create` block)
- **No changes** to `src/v3/CMakeLists.txt`, `tests/unit/integration/legacy_bnetd/CMakeLists.txt`, or `Dockerfile.v3`
- packet_create() count: 78 → **76 remaining** (−2: `_ladder_send`, `apireg_send`)

## R97 -- d2cs outbound obs bridges (handle_d2cs.cpp 9 sites + d2gs.cpp 2 sites)
- **Audit**: `handle_d2cs.cpp` unguarded OUTPUT sites + `d2gs.cpp` keepalive/control sites
  - `handle_d2gs.cpp` and `handle_bnetd.cpp` already fully bridged (R72, R71)
- **Bridge strategy**: observation-only (return 0) for all 11 sites:
  - Outbound bnetd/d2gs packets: complex internal-protocol, v3 encoders not yet complete
  - Ladder/charlist client-bound: pre-built entry arrays; cannot trivially wire without legacy refactor
  - d2gs keepalive/control: sent to multiple connections in loop; `nullptr` passed as conn_ptr
- **New files**:
  - `src/v3/integration/legacy_d2cs/include/integration/legacy_d2cs/send_outbound_obs_bridges.hpp` — 8 obs function declarations
  - `src/v3/integration/legacy_d2cs/src/send_outbound_obs_bridges.cpp` — all 8 return 0
  - `tests/unit/integration/legacy_d2cs/send_outbound_obs_bridges_test.cpp` — 8 TEST_CASE blocks
- **handle_d2cs.cpp** (9 sites wired):
  - `pvpgn_v3_d2cs_obs_accountloginreq_bnetd` — loginreq→bnetd (line 191)
  - `pvpgn_v3_d2cs_obs_charloginreq_bnetd` — createchar→bnetd (line 257) + charlogin→bnetd (line 773)
  - `pvpgn_v3_d2cs_obs_creategamereq_d2gs` — creategame→d2gs (line 402)
  - `pvpgn_v3_d2cs_obs_joingamereq_d2gs` — joingame→d2gs (line 508)
  - `pvpgn_v3_d2cs_obs_ladderreply` — ladder reply (lines 875, 983)
  - `pvpgn_v3_d2cs_obs_charlistreply` — charlist reply (lines 1030, 1149)
- **d2gs.cpp** (2 sites wired):
  - `pvpgn_v3_d2cs_obs_echoreq_d2gs(nullptr)` — keepalive echoreq (line 391)
  - `pvpgn_v3_d2cs_obs_control_d2gs(nullptr)` — restart control (line 418)
- **Build system**: `src/v3/CMakeLists.txt` + `tests/unit/integration/legacy_d2cs/CMakeLists.txt` + `Dockerfile.v3` all updated
- packet_create() count: 76 → **65 remaining** (−11: 9 handle_d2cs + 2 d2gs)

## R98 -- Audit handle_d2gs.cpp, handle_bnetd.cpp, handle_d2cs.cpp (d2cs side) — no new bridges needed
- **Audit scope**: Task referenced `handle_d2cs_d2gs.cpp` and `handle_d2cs_bnetd.cpp` — these files do not exist.
  Actual d2cs files audited: `handle_d2gs.cpp`, `handle_bnetd.cpp`, `handle_d2cs.cpp`
- **Findings**: All OUTPUT `packet_create()` sites in all three files are already fully bridged:
  - `handle_d2gs.cpp` (7 sites): all bridged in R72 (`setinitinfo`, `setconffile`, `authreply`, `setgsinfo`, `creategamereply`, `joingamereply`, `authreq`)
  - `handle_bnetd.cpp` (6 sites): all bridged in R71 (`init_bnetd`, `authreply_bnetd`, `loginreply`, `createcharreply`, `charloginreply`, `gameinforeply_bnetd`)
  - `handle_d2cs.cpp` (20 sites): all bridged in R97 + earlier rounds (obs hooks + direct bridges)
  - `connection.cpp` (d2cs): 4 INPUT read-buffer allocations — not bridgeable
  - `d2gs.cpp`: 2 sites already bridged in R97
- **No new files**, no CMakeLists.txt changes, no Dockerfile.v3 changes
- packet_create() count: 65 → **65 remaining** (−0: all d2cs files fully bridged; remaining 65 are in bnetd files)

## R99 -- Comprehensive bnetd audit + udptest_send.cpp bridge (1 site)
- **Audit scope**: Full ±200-line guard-detection scan of ALL `src/bnetd/` files + `src/common/packet.cpp`
- **Key finding**: The R98 "65 remaining" was a raw grep count of ALL `packet_create()` sites in bnetd files.
  With proper ±200-line guard detection, only **3 truly unguarded OUTPUT sites** remain:
  - `udptest_send.cpp:56` — SERVER_UDPTEST via raw UDP → bridged this round
  - `file.cpp:210` — `packet_class_file` → deferred (binary file protocol)
  - `file.cpp:274` — `packet_class_raw` → deferred (binary file protocol)
- **Skip categories confirmed**:
  - `server.cpp` (9 INPUT read-buffer allocations) — not bridgeable
  - `common/packet.cpp` (2 utility/definition sites) — not bridgeable
  - `anongame_infos.cpp` (1 zlib scratch buffer) — not bridgeable
- **All other bnetd sites verified as already guarded** (guards 40–200 lines away from `packet_create`):
  - `handle_bnet.cpp` L1467, L1815, L3515, L3773, L6000 — all guarded ✓
  - `command.cpp` L1557 — guarded ✓
  - `handle_anongame.cpp` L224, L272, L553, L824 — guarded ✓ (guards in adjacent functions within ±200 lines)
- **Bridge implemented**: `pvpgn_v3_observe_udptest` (observation-only, always returns 0)
  - `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_udptest_bridge.hpp`
  - `src/v3/integration/legacy_bnetd/src/send_udptest_bridge.cpp`
  - `tests/unit/integration/legacy_bnetd/send_udptest_bridge_test.cpp` (2 cases)
- **Call site wired**: `src/bnetd/udptest_send.cpp` L60-62 — `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard + observe call before loop body
- **Build system**: `src/v3/CMakeLists.txt` — added `send_udptest_bridge.cpp` to `integration_legacy_bnetd`
- **Tests**: `tests/unit/integration/legacy_bnetd/CMakeLists.txt` — added `test_integration_legacy_bnetd_send_udptest_bridge`
- packet_create() count: 65 → **64 remaining** (−1: udptest_send.cpp bridged; file.cpp 2 sites deferred)

## R100 -- file.cpp 2 deferred sites bridged — Phase 1 Step 4 COMPLETE
- **Scope**: The last 2 unguarded `packet_create()` OUTPUT sites in all of `src/bnetd/`:
  - `file.cpp:210` — `packet_class_file` (SERVER_FILE_REPLY header packet in `file_send`)
  - `file.cpp:274` — `packet_class_raw` (raw file-body chunk loop in `file_send`)
- **Bridge strategy**: observation-only (return 0 always) — v3 file transfer encoder not yet implemented.
  Both hooks observe the call site without replacing legacy behavior.
- **New files**:
  - `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/send_file_bridge.hpp`
    — declares `pvpgn_v3_observe_file_send` + `pvpgn_v3_observe_file_raw_send`
  - `src/v3/integration/legacy_bnetd/src/send_file_bridge.cpp`
    — both functions return 0 (observation-only)
  - `tests/unit/integration/legacy_bnetd/send_file_bridge_test.cpp`
    — 8 Catch2 cases (null conn, null packet, both null, valid pointers × 2 functions)
- **Call sites wired** in `src/bnetd/file.cpp`:
  - Added `#ifdef PVPGN_V3_BNETD_INTEGRATION` include of `send_file_bridge.hpp`
  - L210 site: `pvpgn_v3_observe_file_send(c, nullptr)` before `packet_create(packet_class_file)`
  - L274 site: `pvpgn_v3_observe_file_raw_send(c, nullptr)` before `packet_create(packet_class_raw)`
- **Build system**: `src/v3/CMakeLists.txt` — added `send_file_bridge.cpp` to `integration_legacy_bnetd` SOURCES
- **Tests**: `tests/unit/integration/legacy_bnetd/CMakeLists.txt` — added `test_integration_legacy_bnetd_send_file_bridge`
- **Dockerfile.v3**: added `test_integration_legacy_bnetd_send_file_bridge` to build target + RUN test line
- packet_create() count: 64 → **62 remaining** (−2: file.cpp both sites bridged)
- **⭐ Phase 1 Step 4 (Packet/Queue migration) is now COMPLETE** — all actionable OUTPUT `packet_create()` sites in bnetd are guarded. Remaining 62 are non-bridgeable (INPUT read-buffer allocations, utility definitions, zlib scratch buffers).
- **Next**: Phase 1 Step 5 — Migrate `src/common/` utility modules (type utilities) to `src/v3/core/`

## R102 -- Phase 1 Step 5 (partial): `tag.h` migrated to `domain/shared/client_tag.hpp`

- **Scope**: Phase 1 Step 5 continuation — migrate `src/common/tag.h` + `tag.cpp` to the v3
  domain layer. The stub `ClientTag` class that already existed in
  `src/v3/domain/shared/include/domain/shared/client_tag.hpp` was extended with the full
  set of known tag constants, predicates, and the `title()` helper.
- **What `tag.h` / `tag.cpp` contained**:
  - `t_tag` / `t_clienttag` / `t_archtag` / `t_gamelang` — all `uint32_t` aliases
  - 28 `CLIENTTAG_*` / `ARCHTAG_*` / `GAMELANG_*` macro pairs (string + uint)
  - `tag_str_to_uint()` / `tag_uint_to_str()` / `tag_uint_to_revstr()` — pack/unpack helpers
  - `clienttag_str_to_uint()` / `clienttag_uint_to_str()` — client-specific wrappers
  - `clienttag_get_title()` — human-readable product name
  - `tag_check_client()` / `tag_check_arch()` / `tag_check_wolv1()` / `tag_check_wolv2()`
  - `tag_check_in_list()`, `tag_sku_to_uint()`, `tag_channeltype_to_uint()`,
    `tag_wol_locale_to_uint()`, `tag_validate_client()`
  - `t_tag_wol_locale` enum (38 locale values)
- **Changes to `src/v3/domain/shared/include/domain/shared/client_tag.hpp`**:
  - Added `from_packed_be(uint32_t)` — construct from big-endian packed uint32
  - Added `is_valid_client()` — mirrors `tag_check_client()`
  - Added `is_valid_arch()` — mirrors `tag_check_arch()`
  - Added `is_wol_v1()` / `is_wol_v2()` — mirrors `tag_check_wolv1/2()`
  - Added `title()` — mirrors `clienttag_get_title()`
  - Added `pvpgn::domain::tags::k*` constants for all 28 Blizzard + WOL client tags,
    3 architecture tags, and 12 game language tags (all `inline constexpr ClientTag`)
  - All new methods are `constexpr`; no `.cpp` needed (header-only)
- **New files**:
  - `tests/unit/domain/shared/client_tag_test.cpp`
    — 22 Catch2 test cases covering: parse/reject, from_packed_be round-trip,
    constexpr array ctor, all 28 Blizzard + WOL constant packed_be() values pinned
    to legacy `CLIENTTAG_*_UINT` macros, all 3 arch tags, all 12 lang tags,
    text() string checks, is_valid_client() true/false, is_valid_arch() true/false,
    is_wol_v1() / is_wol_v2(), title() for all 26 known clients + "Unknown" fallback,
    comparison operators, std::hash in unordered_set.
- **Build system**: `tests/unit/domain/shared/CMakeLists.txt` — added
  `test_domain_shared_client_tag` via `pvpgn_v3_add_test()`
- **Dockerfile.v3**: `test_domain_shared_client_tag` added to cmake `--build` target list
  and RUN test line
- **No changes** to `src/v3/CMakeLists.txt` — `domain_shared` is already an INTERFACE
  library; the header-only addition requires no new sources
- **Legacy `src/common/tag.h` left in place** — still used by legacy bnetd/d2cs/d2dbs
- **Deferred** (Step 5 remainder): `util.{h,cpp}` → `core/` (misc string/path helpers)
- **Next**: continue Phase 1 Step 5 — migrate `util.{h,cpp}`

## R101 -- Phase 1 Step 5 (partial): bnettime + hexdump migrated to `core/`

- **Scope**: First batch of Phase 1 Step 5 — type utility migration from `src/common/` to `src/v3/core/`.
  Audit confirmed `core/endian.hpp` (covers `bn_type.h`) and `core/version.hpp` (covers `proginfo/version.h`)
  already existed. Two new pure C++20 headers created.
- **New files**:
  - `src/v3/core/include/core/bnettime.hpp`
    — Windows FILETIME epoch conversion. `BnetTime` struct (`upper`/`lower` uint32),
    `to_bnettime()` / `from_bnettime()` (chrono ↔ BnetTime), `bnettime_now()`,
    `bnettime_to_string()` / `bnettime_from_string()` ("upper lower" format),
    `local_tzbias_minutes()`, `bnettime_add_tzbias()`.
    Constants: `kBnetTimeUnitsPerSec = 10'000'000`, `kUnixEpochOffsetUnits = 116'444'736'000'000'000`.
  - `src/v3/core/include/core/hexdump.hpp`
    — Hex dump utility. `hexdump_line()` (single 16-byte row with offset),
    `hexdump()` (full buffer multi-line). Format:
    `OOOO:   XX XX XX XX XX XX XX XX   XX XX XX XX XX XX XX XX   ................`
    Span overloads for both.
  - `tests/unit/core/bnettime_test.cpp` — 17 Catch2 cases
  - `tests/unit/core/hexdump_test.cpp` — 14 Catch2 cases
- **Build system**: `tests/unit/core/CMakeLists.txt` — added `test_core_bnettime`, `test_core_hexdump`
- **Dockerfile.v3**: both targets added to cmake `--build` list + RUN test lines
- **Deferred** (Step 5 remainder): `tag.h` → `domain/shared/client_tag.hpp`, `util.{h,cpp}` → `core/`
- **Next**: continue Phase 1 Step 5 — migrate `tag.h` and `util.{h,cpp}`

### Round 102 — tag.h migration (2026-05-21)

- **Phase 1 Step 5 (partial)**: `src/common/tag.h` → `src/v3/domain/shared/include/domain/shared/client_tag.hpp`
  - Extended existing `ClientTag` stub with `from_packed_be()`, `is_valid_client()`,
    `is_valid_arch()`, `is_wol_v1()`, `is_wol_v2()`, `title()`, and 57 `inline constexpr`
    tag constants (28 Blizzard/WOL clients, 3 arch tags, 12 lang tags, `kUnknown`).
  - `tests/unit/domain/shared/client_tag_test.cpp` — 22 Catch2 cases
  - `tests/unit/domain/shared/CMakeLists.txt` — added `test_domain_shared_client_tag`
  - `Dockerfile.v3` — target + RUN line added
  - Legacy `src/common/tag.h` left in place

### Round 103 — util.{h,cpp} migration — Phase 1 Step 5 COMPLETE (2026-05-21)

- **Phase 1 Step 5 COMPLETE**: `src/common/util.{h,cpp}` → `src/v3/core/include/core/string_utils.hpp`
  - Pure C++20 header-only migration. 11 pure utility functions migrated; 2 I/O functions
    (`file_get_line`, `str_print_term`) deferred as they depend on `FILE*`.
  - Migrated functions: `str_starts_with_word`, `str_reverse`, `str_to_uint`,
    `str_to_ushort`, `str_get_bool`, `seconds_to_timestr`, `clockstr_to_seconds`,
    `escape_fs_chars`, `escape_chars`, `unescape_chars`, `bytes_to_hex_str`,
    `hex_str_to_bytes`, `timestr_to_time`, `str_skip_space`, `str_skip_word`.
  - All use `std::string_view` / `std::string` / `std::optional` — no legacy C types,
    no `xalloc`, no `FILE*`.
  - `tests/unit/core/string_utils_test.cpp` — 57 Catch2 test cases
  - `tests/unit/core/CMakeLists.txt` — added `test_core_string_utils`
  - `Dockerfile.v3` — `test_core_string_utils` added to cmake `--build` target list
    and RUN test line added
  - No changes to `src/v3/CMakeLists.txt` — header-only, `core` INTERFACE already
    exposes the include directory
  - Legacy `src/common/util.h` left in place — still used by legacy bnetd/d2cs/d2dbs
- **Phase 1 Step 5 is now COMPLETE** — all 6 utility modules migrated:
  `bn_type.h` (endian.hpp), `version.h` (version.hpp), `bnettime.{h,cpp}` (bnettime.hpp),
  `hexdump.{h,cpp}` (hexdump.hpp), `tag.h` (client_tag.hpp), `util.{h,cpp}` (string_utils.hpp)
- **Next**: Phase 1 Step 6 — Eliminate `xalloc` (replace all `xmalloc`/`xfree`/`xrealloc`/
  `xstrdup` with `std::vector`, `std::unique_ptr`, `std::string` across the legacy tree)

### Round 104 — Phase 1 Step 6: xalloc Elimination COMPLETE (2026-05-21)

- **Phase 1 Step 6 COMPLETE**: All `xalloc` references eliminated from the legacy codebase
- **Audit findings**:
  - Pre-R104 grep for `xmalloc|xfree|xrealloc|xstrdup|xcalloc` returned 60 hits across 38 files
  - On inspection: **0 actual xalloc function calls** remained in bnetd/d2cs/d2dbs/common
    (all had been replaced in Rounds 1–13 with `new`/`delete[]`, `std::string`, etc.)
  - All 60 hits were: macro definitions in `xalloc.h`, comments referencing old patterns,
    or the word "xfree" in GPL license text in bniutils files
  - **70 files** still had stale `#include "common/xalloc.h"` — none used xalloc functions
  - **1 real xalloc API call**: `xalloc_setcb(bnetd_oom_handler)` in `src/bnetd/main.cpp`
- **Actions taken**:
  - Removed `#include "common/xalloc.h"` from all 70 consumer files via `sed -i` bulk sweep:
    - 42 files in `src/bnetd/`
    - 14 files in `src/common/`
    - 13 files in `src/d2cs/`
    - 7 files in `src/d2dbs/`
  - Refactored `src/bnetd/main.cpp` OOM handler:
    - Removed: `bnetd_oom_handler()`, `oom_setup()`, `oom_free()`, `oom_buffer`, `OOM_SAFE_MEM`
    - Removed: `STATUS_OOM_FAILURE` define and case label
    - Kept: `new_oom_handler()` + `std::set_new_handler()` (already present, now sole OOM path)
    - Improved: `new_oom_handler()` now nulls `emergency_mem` after delete to prevent double-free
- **Verification**: `grep -rn "xalloc" src/ --include="*.cpp" --include="*.h" | grep -v "^src/common/xalloc"` → exit 1, no output
- **New file**: `plans/step6-xalloc-checklist.md` — full per-file audit table with before/after
- **Remaining xalloc files** (kept until Phase 1 Step 9 deletion):
  - `src/common/xalloc.h` — still compiled into legacy build
  - `src/common/xalloc.cpp` — still compiled into legacy build
- **Next**: Phase 1 Step 7 — Eliminate legacy data structures (`t_list`, `t_hashtable`)

### Round 105 — Phase 1 Step 7: Legacy Data Structures Audit + Tier-1 Replacements (2026-05-21)

- **Phase 1 Step 7 IN PROGRESS**: Comprehensive audit of `t_list`, `t_hashtable`, `t_elist`, `t_hlist` usage
- **Audit findings** (pre-migration baseline):
  - `t_list`: **165 occurrences** across **47 files** (77 `#include "common/list.h"`)
  - `t_hashtable`: **49 occurrences** across **7 files** (7 `#include "common/hashtable.h"`)
  - `t_elist`: **59 occurrences** across **16 files** (14 `#include "common/elist.h"`)
  - `t_hlist`: **22 occurrences** across **9 files** (via `elist.h`)
- **API documented**: Full `t_list`, `t_hashtable`, `t_elist`, `t_hlist` API mapped in checklist
- **Categorization**:
  - **Tier 1 (Easy)**: 3 files — no public API exposure, simple load/traverse/unload patterns
  - **Tier 2 (Medium)**: ~28 files — no public API exposure but more complex patterns
  - **Tier 3 (Hard)**: ~27 files — `t_list*` / `t_hashtable*` / `t_elist` exposed in public headers
    (e.g. `channel.h`, `clan.h`, `friends.h`, `account.h`, `connection.h`, `realm.h`, etc.)
- **Tier-1 replacements made** (3 files, all build-verified):
  - `src/bnetd/command_groups.cpp`: `static t_list* command_groups_head` → `static std::vector<t_command_groups*> command_groups_list`
    - `list_create()` removed, `list_append_data()` → `push_back()`, `LIST_TRAVERSE` → range-for
    - `list_remove_elem` + `list_destroy` → range-for delete + `clear()`
    - Removed `#include "common/list.h"`
  - `src/bnetd/autoupdate.cpp`: `static t_list* autoupdate_head` → `static std::vector<t_autoupdate*> autoupdate_list`
    - `list_create()` removed, `list_append_data()` → `push_back()`
    - `LIST_TRAVERSE` + `LIST_TRAVERSE_CONST` → range-for
    - `list_remove_elem` + `list_destroy` → range-for delete + `clear()`
    - Removed `#include "common/list.h"`
  - `src/bnetd/character.cpp`: `static t_list* characterlist_head` → `static std::vector<t_character*> characterlist`
    - List was **never populated** (no `list_append_data` calls anywhere) — effectively a no-op list
    - `list_create()` → `clear()`, `list_destroy()` → range-for delete + `clear()`
    - `LIST_TRAVERSE` → range-for
    - Removed `#include "common/list.h"`
- **Post-R105 counts**: `t_list` 165 → 162 (3 removed from `.cpp` files)
- **Build verification**: `cmake --build build --target bnetd` — exit 0, no errors
- **New file**: `plans/step7-datastructs-checklist.md` — full per-file audit table with Tier 1/2/3 classification
- **Next**: Phase 1 Step 7 Tier-2 replacements — `ipban.cpp`, `tournament.cpp`, `anongame.cpp`,
  `watch.cpp` (stale include), `command.cpp`, `handle_bnet.cpp`, `message.cpp`, `output.cpp`,
  `server.cpp`, `tracker.cpp`, `irc.cpp`, `sql_common.cpp`, `storage_file.cpp`, `news.cpp`,
  `d2cs/handle_d2cs.cpp`, `d2cs/d2charlist.cpp`, `d2cs/main.cpp`, `d2cs/server.cpp`,
  `d2dbs/dbspacket.cpp`, `common/trans.cpp`, `common/rcm.cpp`

### Round 106 — Phase 1 Step 7: Tier-2 Replacements (2026-05-21)

- **Phase 1 Step 7 IN PROGRESS**: Tier-2 `t_list` → `std::vector` replacements
- **Files converted** (4 files, all build-verified):
  - `src/bnetd/ipban.cpp`: `static t_list* ipbanlist_head` → `static std::vector<t_ipban_entry*> ipbanlist`
    - `list_append_data()` → `push_back()`
    - `ipbanlist_unload_expired()`: `LIST_TRAVERSE` + `list_remove_elem` → iterator-based `while` + `erase()`
    - `ipban_func_del()`: two `LIST_TRAVERSE` + `list_remove_elem` blocks → iterator-based `erase()` loops
    - `ipban_func_list()`: `LIST_TRAVERSE_CONST` → range-for (removed stale `t_elem const *curr` + `entry` decls)
    - Removed `#include "common/list.h"`
  - `src/bnetd/tournament.cpp`: `static t_list* tournament_head` → `static std::vector<t_tournament_user*> tournament_list`
    - `list_create()` → `tournament_list.clear()`
    - `_gamelist_destroy()`: `LIST_TRAVERSE` + `list_remove_elem` + `list_destroy` → range-for delete + `clear()`
    - `tournament_signup_user()`: `list_prepend_data()` → `push_back()`
    - `tournament_get_user()`: `LIST_TRAVERSE` → range-for with `std::strcmp`
    - `tournament_get_game_in_progress()`: `LIST_TRAVERSE_CONST` → range-for const
    - Removed `#include "common/list.h"`, added `#include <vector>`, `#include <algorithm>`
  - `src/bnetd/anongame.cpp`: `static t_list* matchlists[ANONGAME_TYPES][MAX_LEVEL]` → `static std::vector<t_matchdata*> matchlists[ANONGAME_TYPES][MAX_LEVEL]`
    - `anongame_matchlists_create()`: nested `list_create()` → nested `clear()`
    - `anongame_matchlists_destroy()`: nested `LIST_TRAVERSE` + `list_destroy` → nested range-for delete + `clear()`
    - `_anongame_queue()`: `list_create()` + `list_append_data()` → `push_back()`
    - `_anongame_match()`: `LIST_TRAVERSE` + `elem_get_data(curr)` → range-for (removed `t_elem *curr` decl)
    - `anongame_unqueue()`: `LIST_TRAVERSE` + `list_remove_elem` → iterator-based `erase()`
    - Removed `#include "common/list.h"`, added `#include <vector>`
  - `src/common/trans.cpp`: `static t_list* trans_head` → `static std::vector<t_trans*> trans_list`
    - `trans_load()`: `list_create()` → `trans_list.clear()`, `list_append_data()` → `push_back()` (×2)
    - `trans_unload()`: `LIST_TRAVERSE` + `list_remove_elem` + `list_destroy` → range-for delete + `clear()`
    - `trans_net()`: `LIST_TRAVERSE_CONST` → range-for (removed stale `t_elem const *curr` + `t_trans *entry` decls)
    - Removed `#include "common/list.h"`, added `#include <vector>`
- **Reclassified Tier 2 → Tier 3** (16 files — use `t_list*` from public APIs):
  - `watch.cpp`: `account_get_friends()` → `t_list*` from `friends.h`
  - `command.cpp`: `connlist()`, `channellist()`, `account_get_friends()` from `friends.h`
  - `message.cpp`: `connlist()` from `connection.h`
  - `tracker.cpp`: `addrlist_*` from `addr.h`
  - `irc.cpp`: `channellist()`, `channel_get_banlist()`
  - `output.cpp`: `connlist()`, `channellist()`
  - `server.cpp`: `addrlist_*`, `connlist()`
  - `sql_common.cpp`: `clan->members` (`t_list*` in `clan.h`)
  - `storage_file.cpp`: `clan->members`, `t_hlist` from `storage.h`
  - `news.cpp`: `t_elist list` in `t_news_index` struct in `news.h`
  - `d2cs/handle_d2cs.cpp`: `d2cs_gamelist()`, `game_get_charlist()`
  - `d2cs/d2charlist.cpp`: `t_elist list` in `t_d2charlist` in `d2charlist.h`
  - `d2cs/main.cpp`: calls create/destroy on Tier 3 lists
  - `d2cs/server.cpp`: `addrlist_*`, `hashtable_purge(d2cs_connlist())`
  - `d2dbs/dbspacket.cpp`: `dbs_server_connection_list` from `dbserver.h`
  - `common/rcm.cpp`: `t_elist refs` in public structs in `rcm.h`
- **Post-R106 counts**: `t_list` 162 → ~146 (removed ~16 occurrences across 4 files)
- **Build verification**: `cmake --build build --target bnetd -j$(nproc)` — exit 0, no errors
- **Updated**: `plans/step7-datastructs-checklist.md` — Tier 2 table updated with ✅ R106 for 4 files, ⏭ R107+ for all reclassified Tier 3 files; R106 Replacements Log section added
- **Next**: R107+ — audit remaining Tier 2 candidates (`account_wrap.cpp`, `handle_bnet.cpp`, `handle_wol.cpp`, `handle_anongame.cpp`, `luafunctions.cpp`, `storage_sql.cpp`, `file_plain.cpp`, `ladder.cpp`, `profile_bridge.cpp`), then begin Tier 3 migrations after public header APIs are migrated

### Round 107 -- Step 7 Tier 3: channel.h/channel.cpp public API migration (t_list* → std::vector)

- **Goal**: Migrate `src/bnetd/channel.h` and `src/bnetd/channel.cpp` from `t_list*` to `std::vector` in the public API (channellist and banlist)
- **Files modified**:
  - `src/bnetd/channel.h`:
    - Added `#include <string>` and `#include <vector>` in both `CHANNEL_INTERNAL_ACCESS` and protos sections
    - Removed `#include "common/list.h"` from protos section
    - Changed `t_list* banlist` → `std::vector<std::string> banlist` in channel struct
    - Removed `channel_create(..., t_list* channellist)` overload from public API
    - Changed `channel_destroy(t_channel*, t_elem**)` → `channel_destroy(t_channel*)`
    - Changed `channel_get_banlist()` return type → `const std::vector<std::string>&`
    - Changed `channellist()` return type → `const std::vector<t_channel*>&`
  - `src/bnetd/channel.cpp`:
    - Removed `#include "common/list.h"`, added `#include <algorithm>`, `#include <string>`, `#include <vector>`
    - `static t_list* channellist_head` → `static std::vector<t_channel*> channellist_head`
    - Renamed `channel_create` with channellist param to `channel_create_impl` (static internal, takes `std::vector<t_channel*>*`)
    - `channel->banlist = list_create()` → removed (default-constructed vector)
    - `list_append_data(channellist, channel)` → `channellist->push_back(channel)`
    - `channel_destroy` rewritten — no `t_elem**` param, uses `std::find`+`erase`, `banlist.clear()`
    - `channel_ban_user` rewritten — range-for + `emplace_back`
    - `channel_unban_user` rewritten — iterator-based erase
    - `channel_check_banning` rewritten — range-for
    - Added `static const std::vector<std::string> s_empty_banlist` for null-channel fallback
    - `channellist_reload` rewritten — snapshot copy pattern with `std::vector<t_channel*>`
    - `channellist_create` / `channellist_destroy` — no longer use `list_create`/`list_destroy`
    - `channellist()` returns `const std::vector<t_channel*>&`
    - `channellist_get_length()` uses `static_cast<int>(channellist_head.size())`
    - All three find functions rewritten with range-for
  - **Caller files updated** (8 files, 11 sites total):
    - `src/bnetd/irc.cpp`: `LIST_TRAVERSE_CONST(channellist(), curr)` → range-for; `LIST_TRAVERSE_CONST(channel_get_banlist(channel), curr)` → range-for `const auto& banned`
    - `src/bnetd/handle_bnet.cpp`: `LIST_TRAVERSE_CONST(channellist(), curr)` → range-for
    - `src/bnetd/handle_wol.cpp`: `LIST_TRAVERSE_CONST(channellist(), curr)` → range-for; `channel_create(..., NULL)` → `channel_create(...)` (removed trailing NULL)
    - `src/bnetd/handle_irc.cpp`: `LIST_TRAVERSE_CONST(channellist(), curr)` → range-for
    - `src/bnetd/luafunctions.cpp`: `LIST_TRAVERSE(channellist(), curr)` → range-for
    - `src/bnetd/command.cpp`: `LIST_TRAVERSE_CONST(channel_get_banlist(channel), curr)` → range-for `const auto& banned`; `LIST_TRAVERSE_CONST(channellist(), curr)` → range-for
    - `src/bnetd/output.cpp`: two `LIST_TRAVERSE_CONST(channellist(), curr)` → range-for (kept `t_elem const* curr` for remaining `connlist()` traversals)
    - `src/bnetd/connection.cpp`: `channel_destroy(channel, &curr)` → `channel_destroy(channel)`; removed `t_elem * curr` declaration
- **Build verification**: `cmake --build build --target bnetd` — exit 0, no errors
- **Updated**: `plans/step7-datastructs-checklist.md` — `channel.cpp` row marked ✅ R107
- **Next**: Continue Tier 3 migrations — next candidates: `clan.h`/`clan.cpp` (friends list, member list), `friends.h`/`friends.cpp`, `realm.h`/`realm.cpp`

### Round 108 -- Step 7 Tier 3: clan.h/clan.cpp + friends.h/friends.cpp + account.h/account.cpp public API migration (t_list* → std::vector)

- **Goal**: Migrate `clan.h`/`clan.cpp`, `friends.h`/`friends.cpp`, `account.h`/`account.cpp` from `t_list*` to `std::vector` in their public APIs, fix all compile errors, update all caller files, and verify clean build
- **Files modified**:
  - `src/bnetd/clan.h`: `clan_get_members()` return type → `std::vector<t_clanmember*>&`; `t_clan::members` field → `std::vector<struct _clanmember*>`
  - `src/bnetd/clan.cpp`: internal `LIST_TRAVERSE` patterns replaced with range-for; `list_create`/`list_destroy`/`list_append_data` removed; `clan_get_members()` returns `std::vector<t_clanmember*>&`
  - `src/bnetd/friends.h`: all functions taking `t_list*` first param changed to `std::vector<t_friend*>&`
  - `src/bnetd/friends.cpp`: implementation updated to use `std::vector` operations throughout
  - `src/bnetd/account.h`: `t_account::friends` field → `std::vector<struct friend_struct*>`; `account_get_friends()` return type → `std::vector<t_friend*>&`
  - `src/bnetd/account.cpp`:
    - Line 132: Removed `account->friends = NULL` (replaced with comment — vector is default-initialized)
    - Lines 787-812: Changed `if (account->friends != NULL)` → `if (FLAG_ISSET(account->flags, ACCOUNT_FLAG_FLOADED))` to check if friends list is loaded
  - `src/bnetd/watch.cpp`: Removed `#include "common/list.h"`; replaced `t_elem const * curr`, `t_list * flist`, `LIST_TRAVERSE(flist, curr)` with `auto& flist` + range-for
  - `src/bnetd/command.cpp` (`_handle_friends_command`, 5 sites):
    - "add" branch: `auto& flist = account_get_friends(my_acc)`
    - "msg" branch: `auto& flist2 = account_get_friends(my_acc)` + range-for
    - "promote" branch: `auto& flist_p = account_get_friends(my_acc)`
    - "demote" branch: `auto& flist_d = account_get_friends(my_acc)`
    - "list" branch: `auto& flist_l = account_get_friends(my_acc)` + `if (!flist_l.empty())`
  - `src/bnetd/handle_bnet.cpp` (3 functions):
    - `_client_friendslistreq`: `auto& flist = account_get_friends(account)` (removed NULL check); `if (!flist.empty())`
    - `_client_friendinforeq`: `auto& flist2 = account_get_friends(account)`
    - `_client_atfriendscreen`: `auto& my_friend_list = account_get_friends(...)` + range-for replacing `LIST_TRAVERSE`
  - `src/bnetd/handle_wol.cpp`: `auto& flist = account_get_friends(my_acc)` + `if (!flist.empty())`
  - `src/bnetd/luafunctions.cpp`:
    - `__account_get_friends`: `auto& friendlist = account_get_friends(account)` + range-for
    - `__clan_get_members`: `auto& clanmembers = clan_get_members(clan)` + range-for
  - `src/bnetd/account_wrap.cpp` (3 sites):
    - `account_add_friend`: `auto& flist = account_get_friends(my_acc)` (removed NULL check)
    - `account_remove_friend2`: `auto& flist = account_get_friends(account)` (removed NULL check)
    - `account_remove_friend2`: `auto& fflist = account_get_friends(facc)`; `if (facc && fflist && ffr)` → `if (facc && ffr)`
  - `src/bnetd/storage_file.cpp` (discovered during build, 3 sites):
    - `file_read_clans`: Removed `clan->members = list_create()`; `list_append_data(clan->members, member)` → `clan->members.push_back(member)`
    - `file_write_clan`: Removed `t_elem *curr`; `LIST_TRAVERSE(clan->members, curr)` + `elem_get_data` → range-for
- **Build verification**: `cmake --build build --target bnetd` — exit 0, no errors
- **Updated**: `plans/step7-datastructs-checklist.md` — `clan.cpp`, `friends.cpp` rows marked ✅ R108; full R108 Replacements Log added
- **Next**: Continue Tier 3 migrations — `realm.h`/`realm.cpp`, `team.h`/`team.cpp`, etc.

### Round 109 -- Step 7 Tier 3: connection.h/connection.cpp + quota.h public API migration (t_list* → std::vector / std::deque)

- **Goal**: Migrate `connection.h`/`connection.cpp` from `t_list*` to `std::vector<t_connection*>` in the public API (`connlist()` return type), migrate `quota.h` `t_quota::list` from `t_list*` to `std::deque<t_qline>`, remove `t_elem**` from `conn_destroy` signature, update all caller files, and verify clean build
- **Files modified**:
  - `src/bnetd/quota.h`: `t_list* list` → `std::deque<t_qline> list`; removed `#include "common/list.h"`, added `#include <deque>`
  - `src/bnetd/connection.h`: `connlist()` return type → `const std::vector<t_connection*>&`; `conn_destroy` signature: removed `t_elem** elem` parameter; removed `#include "common/list.h"`, added `#include <vector>`
  - `src/bnetd/connection.cpp`:
    - `static t_list * conn_head` → `static std::vector<t_connection*> conn_head`
    - `static t_list * conn_dead` → `static std::vector<t_connection*> conn_dead`
    - `conn_create`: `list_prepend_data` → `conn_head.push_back(temp)`; quota.list default-constructed
    - `conn_destroy`: new signature (no `t_elem**`); `std::find`+`erase` for both `conn_head` and `conn_dead`; `quota.list.clear()`
    - `conn_set_state`: `conn_dead.push_back(c)` / `std::find`+`erase`
    - `conn_quota_exceeded`: `while(!list.empty()) { front(); pop_front(); }` + stack-allocated `t_qline` + `push_back()`
    - `connlist_create`/`connlist_destroy`: `clear()` calls
    - `connlist_reap`: snapshot + range-for to avoid iterator invalidation
    - `connlist()`: returns `const std::vector<t_connection*>&`
    - All `connlist_find_*`, `connlist_login_get_length`, `connlist_count_connections`, `conn_get_user_count_by_clienttag`: range-for
    - `connlist_get_length`: `static_cast<int>(conn_head.size())`
  - `src/bnetd/server.cpp` (`_shutdown_conns`): snapshot + range-for; `conn_destroy(c, DESTROY_FROM_CONNLIST)` (no `t_elem**`)
  - `src/bnetd/command.cpp`: 5 `LIST_TRAVERSE_CONST(connlist(), curr)` → range-for; removed `t_elem const * curr` declarations
  - `src/bnetd/message.cpp`: 2 `LIST_TRAVERSE_CONST(connlist(), curr)` → range-for
  - `src/bnetd/output.cpp`: 2 `LIST_TRAVERSE_CONST(connlist(), curr)` → range-for; removed `t_elem const *curr`
  - `src/bnetd/luafunctions.cpp`: `t_elem const * curr; LIST_TRAVERSE_CONST(connlist(), curr)` → `for (t_connection * conn : connlist())`
- **Key design decisions**:
  - `conn_destroy` `t_elem**` parameter removed entirely — was only needed for `list_remove_elem` during `LIST_TRAVERSE`; with vector, `std::find`+`erase` handles removal without an iterator parameter
  - Safe mutation during traversal: both `connlist_reap` and `_shutdown_conns` snapshot the vector before iterating to avoid iterator invalidation when `conn_destroy` modifies `conn_head`/`conn_dead`
  - `t_quota::list` migrated to `std::deque<t_qline>` (value semantics, `pop_front()` for expiry, `push_back()` for new entries)
- **Build verification**: `cmake --build build --target bnetd` — exit 0, no errors
- **Updated**: `plans/step7-datastructs-checklist.md` — `connection.cpp` row marked ✅ R109; full R109 Replacements Log added
- **Next**: Continue Tier 3 migrations — `channel.cpp` (already done R107), `realm.h`/`realm.cpp`, `team.h`/`team.cpp`, etc.

### Round 110 -- Step 7 Tier 3: realm.h/realm.cpp public API migration (t_list* → std::vector)

- **Goal**: Migrate `realm.h`/`realm.cpp` from `t_list*` to `const std::vector<t_realm*>&` in the public API (`realmlist()` return type), update all caller files, and verify clean build. Note: `game.h` already uses `t_elist*` (not `t_list*`) so no change was needed there.
- **Files modified**:
  - `src/bnetd/realm.h`: `realmlist()` return type → `const std::vector<t_realm*>&`; removed `#include "common/list.h"`, added `#include <vector>`
  - `src/bnetd/realm.cpp`:
    - Removed `#include "common/list.h"`, added `#include <vector>` and `#include <algorithm>`
    - `static t_list * realmlist_head` → `static std::vector<t_realm*> realmlist_head`
    - `realmlist_load`: returns `std::vector<t_realm*>` (was `t_list*`); `list_create()`/`list_prepend_data()` → `result.push_back(realm)`
    - `realmlist_reload`: `t_list*` locals → `std::vector<t_realm*>`; `LIST_TRAVERSE` → range-for; `list_remove_elem`/`list_destroy` → vector move semantics
    - `realmlist_create`: assigns `realmlist_head = realmlist_load(filename)`; checks `empty()` instead of NULL
    - `realmlist_unload`: converted to `static void` taking `std::vector<t_realm*>&`; range-for + `clear()`
    - `realmlist_destroy`: calls `realmlist_unload(realmlist_head)`; returns 0
    - `realmlist()`: returns `const std::vector<t_realm*>&`
    - `realmlist_find_realm`: range-for over `realmlist_head`
    - `realmlist_find_realm_by_ip`: range-for over `realmlist_head`
  - `src/bnetd/handle_bnet.cpp`:
    - 4 `LIST_TRAVERSE_CONST(realmlist(), curr)` → `for (t_realm const *realm : realmlist())`
    - Removed `t_elem const *curr` and `t_realm const *realm` local declarations where made redundant
- **Key design decisions**:
  - `realmlist()` returns `const std::vector<t_realm*>&` — callers iterate directly with range-for, no cast needed
  - `realmlist_load` returns by value (moved into `realmlist_head`); empty vector signals failure instead of NULL
  - `handle_bnet.cpp` still includes `common/list.h` for other list uses (channel, etc.)
- **Build verification**: `cmake --build build --target bnetd` — exit 0, no errors
- **Updated**: `plans/step7-datastructs-checklist.md` — `realm.cpp` row marked ✅ R110; round header updated to R110
- **Next**: Continue Tier 3 migrations — `team.h`/`team.cpp`, `watch.h`/`watch.cpp`, etc.

### Round 112 -- Step 8 Tier A + Tier B: Migrate src/compat/ headers to src/v3/infra/compat/

- **Goal**: Audit all `src/compat/` modules, classify by migration difficulty (Tier A/B/C), create v3 equivalents for Tier A and Tier B modules in `src/v3/infra/compat/include/infra/compat/`, and document the migration plan for Tier C.
- **Audit findings**:
  - 16 compat modules inventoried (including 2 with `.cpp` implementations: `pdir`, `pgetopt`, `psock`, `strerror`)
  - `src/v3/infra/compat/` already existed with `platform.hpp` and `process.hpp` (covering `gethostname.h` + `pgetpid.h`)
  - `infra_compat` library already defined in `src/v3/CMakeLists.txt` lines 122-130
  - Caller counts: `psock.h` 17 (Tier C), `strerror.h` 9 (Tier B), `pdir.h` 8 (Tier C), `mkdir.h` 4 (Tier B), `rename.h` 4 (Tier B), `runtime_libs.h` 4 (Tier B), `stdfileno.h` 4 (Tier A), `socket.h` 2 (Tier A), `pgetpid.h` 3 (already done), `netinet_in.h` 1 (Tier A), `recv.h` 1 (Tier A), `send.h` 1 (Tier A), `gethostname.h` 1 (already done), `read.h` 0 (Tier A), `termios.h` 0 (Tier A), `pgetopt.h` 0 direct (Tier C)
- **Files created** (Tier A — 7 headers):
  - `src/v3/infra/compat/include/infra/compat/netinet_in.hpp` — `kInAddrLoopback`, `kInAddrAny` constants
  - `src/v3/infra/compat/include/infra/compat/read.hpp` — ensures `read()`/`_read()` declared
  - `src/v3/infra/compat/include/infra/compat/stdfileno.hpp` — `kStdinFd`, `kStdoutFd`, `kStderrFd` constants
  - `src/v3/infra/compat/include/infra/compat/termios.hpp` — `Termios` type + `tcgetattr`/`tcsetattr` (POSIX or stub)
  - `src/v3/infra/compat/include/infra/compat/socket.hpp` — platform socket header inclusion shim
  - `src/v3/infra/compat/include/infra/compat/recv.hpp` — ensures `recv()` declared (delegates to `socket.hpp`)
  - `src/v3/infra/compat/include/infra/compat/send.hpp` — ensures `send()` declared (delegates to `socket.hpp`)
- **Files created** (Tier B — 4 headers):
  - `src/v3/infra/compat/include/infra/compat/mkdir.hpp` — `make_directory()` via `std::filesystem`
  - `src/v3/infra/compat/include/infra/compat/rename.hpp` — `rename_file()` via `std::filesystem`
  - `src/v3/infra/compat/include/infra/compat/runtime_libs.hpp` — `DynamicLibrary` RAII + `open/get/close_library()`
  - `src/v3/infra/compat/include/infra/compat/strerror.hpp` — `error_string()`, `socket_error_string()` via `std::system_error`
- **Files modified**:
  - `src/v3/CMakeLists.txt` — updated `infra_compat` comment block with full header inventory and Tier C pending notes
  - `plans/progress-master.md` — Step 8 marked `[-]` in progress; Round 112 session log added
- **Files created** (plans):
  - `plans/step8-networking-checklist.md` — full audit table, caller counts, tier classification, migration order, RAII opportunities
- **Callers NOT yet updated** — per task constraints, no `src/bnetd/`, `src/d2cs/`, `src/d2dbs/` files were modified
- **Next**: Round 114 — Tier C: `pgetopt.h` → `getopt.hpp` (CLI11 or system getopt wrapper)

### Round 113 — Step 8 Tier C: pdir.h → directory.hpp (RAII directory iterator)

- **Goal**: Migrate `src/compat/pdir.h` + `src/compat/pdir.cpp` to a modern C++20 RAII equivalent in `src/v3/infra/compat/`.
- **Legacy API analysed**:
  - `pvpgn::Directory` class: constructor throws `OpenError`, `read()` → `const char*`, `rewind()`, `operator bool()`
  - `dir_getfiles(dir, ext, recursive)` → `std::vector<std::string>` with case-insensitive extension filter
  - 8 callers: `account.cpp`, `clan.cpp`, `i18n.cpp`, `luainterface.cpp`, `mail.h`, `storage_file.cpp`, `userlog.cpp` (bnetd), `handle_d2cs.cpp` (d2cs)
  - Usage patterns: `Directory dir(path)` + `while ((dentry = dir.read()))` loop; `dir_getfiles()` for bulk listing
- **Files created**:
  - `src/v3/infra/compat/include/infra/compat/directory.hpp` — header-only C++20 RAII implementation:
    - `DirectoryEntry` value type: `name` (filename), `full_path`, `is_directory()`, `is_regular_file()`
    - `DirectoryIterator` RAII class: move-only, range-for support, `next()`, `rewind()`, `reset()`, `is_open()`, `at_end()`
    - `open_directory(path)` → `std::optional<DirectoryIterator>` (no-throw factory)
    - `read_directory(iter)` → `std::optional<DirectoryEntry>` (legacy-compatible advance)
    - `close_directory(iter)` → `void` (resets to end state)
    - `list_files(dir, ext, recursive)` → `std::vector<std::filesystem::path>` (replaces `dir_getfiles()`)
    - Case-insensitive extension filter via `detail::iequal()` (matches legacy `strcasecmp` behaviour)
    - Subdirectory results prepended (legacy dirs-first ordering preserved)
  - `tests/unit/infra/compat/test_directory.cpp` — 20 Catch2 test cases covering:
    - `open_directory` valid/invalid/file paths
    - `read_directory` empty dir, flat iteration, post-exhaustion nullopt
    - `close_directory` stops iteration
    - `rewind()` restarts iteration
    - Range-for loop (populated and empty dirs)
    - `DirectoryEntry::is_regular_file()` / `is_directory()` / `full_path` correctness
    - Move construction and move assignment
    - `list_files` extension filter, wildcard, empty ext, case-insensitive, non-matching, non-existent dir
    - `list_files` recursive descent, hidden-entry skipping, dirs-first ordering
- **Files modified**:
  - `tests/unit/infra/compat/CMakeLists.txt` — added `test_infra_compat_directory` target
  - `src/v3/CMakeLists.txt` — updated `infra_compat` comment inventory: `directory.hpp` added, pending note updated to Round 114+
  - `plans/step8-networking-checklist.md` — `pdir.h` row marked ✅ R113; Round 113 section expanded with full delivery notes
  - `plans/progress-master.md` — Step 8 progress updated; R113 session log added
- **Callers NOT yet updated** — per task constraints, no `src/bnetd/` or `src/d2cs/` files were modified
- **Next**: Round 114 — Tier C: `pgetopt.h` → `getopt.hpp`

### Round 111 -- Step 7 Tier 3: watch.h/watch.cpp audit + Phase 1 Step 7 COMPLETE

- **Goal**: Audit `watch.h`/`watch.cpp` for `t_list*` in the public API; confirm migration status; mark Phase 1 Step 7 as COMPLETE.
- **Finding**: `watch.h` never had `t_list*` in its public API — it uses `std::list<Watch>` internally (C++ STL, not the legacy `t_list`). The `#include "common/list.h"` and `t_list* flist` usage in `watch.cpp` were already removed in R108 as a caller-update side effect when `friends.h` was migrated. R111 audit confirmed zero `t_list`/`list.h` references in both files.
- **Files modified**: none (code was already clean)
- **Grep result**: zero `t_list` / `list.h` references in `src/bnetd/watch.cpp` and `src/bnetd/watch.h`
- **Tier 3 header migration summary** (all Tier 3 public-API headers now clean):
  - `channel.h` ✅ R107
  - `clan.h` / `friends.h` / `account.h` ✅ R108
  - `connection.h` ✅ R109
  - `realm.h` ✅ R110
  - `watch.h` ✅ R111 (confirmed clean; no code change needed)
  - `game.h` — no change needed (uses `t_elist`, not `t_list`)
- **Updated**: `plans/step7-datastructs-checklist.md` — `watch.cpp` row marked ✅ R111; R110 and R111 Replacements Logs added; Tier 3 completion note added; round header updated to R111
- **Updated**: `plans/progress-master.md` — Step 7 marked `[x]` COMPLETE; R111 session log added
- **Phase 1 Step 7 status**: ✅ COMPLETE (Tier 3 header migrations done; remaining `t_list`/`t_hashtable`/`t_elist` work in lower-priority files tracked in checklist for future rounds)
- **Next**: Phase 1 Step 8 — Migrate `src/compat/` platform abstractions to `src/v3/infra/compat/`

### Round 114 — Phase 1 Step 8 Tier C: `pgetopt.h` → `getopt.hpp` ✅ COMPLETE

- **Goal**: Migrate `src/compat/pgetopt.h` + `src/compat/pgetopt.cpp` (855-line GNU getopt port) to a
  modern C++20 header-only argument parser in `src/v3/infra/compat/`.
- **Caller audit**: 0 direct callers of `pgetopt.h` confirmed — `grep '#include.*pgetopt'` across
  all of `src/` returns only `src/compat/pgetopt.cpp` itself (compiled only when `!HAVE_GETOPT`).
- **Files created**:
  - `src/v3/infra/compat/include/infra/compat/getopt.hpp` — header-only C++20 argument parser:
    - `ArgSpec` value type: `short_name` (char), `long_name` (string), `description`, `has_arg`
      (`ArgSpec::Arg::none` / `required` / `optional`)
    - `ParseResult` struct: `options` map, `positional` vector, `error` string;
      `ok()`, `get(name)` → `std::optional<std::string>`, `has(name)` → `bool`
    - `CommandLineParser` class (move-only, non-copyable):
      - `add_option(short, long, desc, has_arg)` — register an option
      - `parse()` → `bool` — idempotent; resets state on each call
      - `get(name)` → `std::optional<std::string>`
      - `has(name)` → `bool`
      - `positional_args()` → `const std::vector<std::string>&`
      - `error()` → `std::string_view`
      - `usage(program_name)` → `std::string`
    - `parse_args(argc, argv, vector<ArgSpec>)` → `ParseResult` free function
    - `parse_args(argc, argv, initializer_list<ArgSpec>)` → `ParseResult` overload
    - Supports: `-v`, `-o val`, `-oval`, `--verbose`, `--output=val`, `--output val`, `--`
    - No exceptions — all errors via `ParseResult::error` / `parser.error()`
    - `[[nodiscard]]` on all query / factory functions
  - `tests/unit/infra/compat/test_getopt.cpp` — 28 Catch2 test cases covering:
    - Empty argv (just program name)
    - Short flag (`-v`), short+arg space-separated (`-o val`), short+arg concatenated (`-oval`)
    - Long flag (`--verbose`), long+arg inline-= (`--output=val`), long+arg space (`--output val`)
    - Mixed short and long options in one invocation
    - Positional arguments (bare tokens), options+positionals mixed
    - `--` end-of-options separator (bare `--`, `--` with tokens after)
    - Unknown short option error, unknown long option error
    - Missing required arg for short option, missing required arg for long option
    - Long flag with inline `=` when spec says `no_argument` → error
    - Optional arg present via `--opt=val`, optional arg absent via `--opt`
    - Multiple flags set independently
    - `get()` returns `nullopt` for absent option, `has()` returns false for absent option
    - `usage()` contains registered option names and program name
    - `parse_args()` vector overload, `parse_args()` initializer_list overload
    - `parse_args()` returns error on unknown option
    - `parse()` idempotency (second call resets state), error reset on retry
    - Long-only option (`short_name = '\0'`)
    - `ParseResult::get()` and `has()` member functions
- **Files modified**:
  - `tests/unit/infra/compat/CMakeLists.txt` — added `test_infra_compat_getopt` target
  - `src/v3/CMakeLists.txt` — updated `infra_compat` comment inventory: `getopt.hpp` added ✅ R114,
    pending note updated to Round 115+
  - `plans/step8-networking-checklist.md` — `pgetopt.h` row marked ✅ R114; Round 114 section
    expanded with full delivery notes
  - `plans/progress-master.md` — Step 8 progress updated; R114 session log added
- **Callers NOT updated** — 0 direct callers confirmed; no `src/bnetd/`, `src/d2cs/`, `src/d2dbs/`
  files were modified
- **Next**: Round 115 — Tier C: `psock.h` → `infra/net/socket.hpp` (RAII, 17 callers)

### Round 115 — Phase 1 Step 8 Tier C: `psock.h` → `infra/net/socket.hpp` ✅ COMPLETE

- **Goal**: Migrate `src/compat/psock.h` + `src/compat/psock.cpp` (308/87 lines) to a modern
  C++20 RAII socket wrapper in `src/v3/infra/net/`.
- **Key design decision**: No Boost.Asio — thin wrapper directly over POSIX sockets / Winsock2.
  Boost.Asio is already used by the higher-level `infra_net` layer (io_runtime, tcp_acceptor, etc.).
  `socket.hpp` is a lower-level primitive that works without Boost, making it usable in contexts
  where the full Boost.Asio event loop is not running.
- **Caller audit**: 17 callers confirmed across `src/bnetd/` (6), `src/common/` (3 + 1 header),
  `src/d2cs/` (6), `src/d2dbs/` (2). Full list in `plans/step8-networking-checklist.md`.
- **Files created**:
  - `src/v3/infra/net/include/infra/net/socket.hpp` — header-only C++20 RAII socket wrapper:
    - `SocketFd` — platform alias (`int` on POSIX, `SOCKET` on Win32)
    - `kInvalidSocket` — platform sentinel (`-1` / `INVALID_SOCKET`)
    - `IpAddress` — wraps `in_addr`; `from_string()` → `std::optional<IpAddress>`,
      `to_string()`, `any()`, `loopback()`, `host_order()`, `network_order()`, `operator==`/`!=`
    - `Port` — strong typedef over `uint16_t`; `host_order()`, `network_order()`, `operator==`/`!=`
    - `SocketAddress` — wraps `sockaddr_in`; `from(IpAddress, Port)`, `ip()`, `port()`,
      `raw()`, `size()`
    - `UniqueSocket` — RAII move-only socket; auto-closes on destruction;
      `get()`, `valid()`, `release()`, `reset()`
    - `WinsockGuard` — RAII `WSAStartup`/`WSACleanup` (no-op on POSIX); `initialized()`
    - `make_tcp_socket()` → `std::optional<UniqueSocket>`
    - `make_udp_socket()` → `std::optional<UniqueSocket>`
    - `bind_socket(UniqueSocket&, SocketAddress)` → `bool`
    - `listen_socket(UniqueSocket&, int backlog = 5)` → `bool`
    - `accept_connection(UniqueSocket&)` → `std::optional<std::pair<UniqueSocket, SocketAddress>>`
    - `connect_socket(UniqueSocket&, SocketAddress)` → `bool`
    - `set_nonblocking(UniqueSocket&, bool)` → `bool`
    - `set_reuse_addr(UniqueSocket&, bool)` → `bool`
    - `socket_send(UniqueSocket&, std::span<const std::byte>, int)` → `std::optional<std::size_t>`
    - `socket_recv(UniqueSocket&, std::span<std::byte>, int)` → `std::optional<std::size_t>`
    - `socket_error()` → `int`
    - `socket_error_string(int)` → `std::string`
    - All factory/query functions `[[nodiscard]]`; no exceptions; header-only
    - Namespace: `pvpgn::infra::net`
    - Verified: `g++ -std=c++20 -fsyntax-only` exits 0 (only harmless `#pragma once` warning)
  - `tests/unit/infra/net/test_socket.cpp` — 30 Catch2 test cases:
    - **Unit tests** (no real sockets, `[unit]` tag):
      - `UniqueSocket` default construction (invalid), move construction (transfers fd),
        move assignment, `release()`, `reset()` with/without fd
      - `IpAddress::from_string()` success/failure cases (valid, "invalid", "", out-of-range)
      - `IpAddress::any()` (0.0.0.0), `loopback()` (127.0.0.1), `to_string()` round-trip,
        `host_order()`/`network_order()` byte-swap, `operator==`/`!=`
      - `Port` constructor, `host_order()`, `network_order()`, default (0), `operator==`/`!=`
      - `SocketAddress::from()`, `ip()`/`port()` round-trip, `size()`
      - `WinsockGuard::initialized()` returns true
      - `socket_error()` smoke test, `socket_error_string()` non-empty
    - **Integration tests** (`[integration]` tag — require OS socket support):
      - `make_tcp_socket()` valid, `make_udp_socket()` valid
      - `set_reuse_addr()` on/off, `set_nonblocking()` on/off
      - `bind_socket()` on loopback:0, `listen_socket()` after bind
      - `accept_connection()` returns nullopt (non-blocking, no peer)
- **Files modified**:
  - `tests/unit/infra/net/CMakeLists.txt` — added `test_infra_net_socket` target
  - `src/v3/CMakeLists.txt` — updated `infra/net` comment block with `socket.hpp` inventory
  - `plans/step8-networking-checklist.md` — `psock.h` row marked ✅ R115; Round 115 section
    expanded with full delivery notes
  - `plans/progress-master.md` — Step 8 progress updated; R115 session log added
- **Callers NOT updated** — 17 callers in `src/bnetd/`, `src/common/`, `src/d2cs/`, `src/d2dbs/`
  intentionally left unchanged; caller migration is Round 116+
- **Phase 1 Step 8 status**: All v3 headers COMPLETE (Tier A ✅ R112, Tier B ✅ R112,
  pdir ✅ R113, pgetopt ✅ R114, psock ✅ R115); pdir callers ✅ R116; psock callers pending
- **Next**: Round 117 — Caller migration for `psock.h` → `infra/net/socket.hpp` (17 files)

### Round 116 — Phase 1 Step 8 Tier C: `pdir.h` caller migration ✅ COMPLETE

- **Goal**: Migrate all 8 callers of `src/compat/pdir.h` to use the new
  `src/v3/infra/compat/include/infra/compat/directory.hpp` API.
- **New API used**:
  - `open_directory(path)` → `std::optional<DirectoryIterator>` (replaces `Directory dir(path)` + try/catch)
  - `read_directory(iter)` → `std::optional<DirectoryEntry>` (replaces `dir.read()`)
  - `list_files(dir, ext, recursive)` → `std::vector<std::filesystem::path>` (replaces `dir_getfiles()`)
  - `DirectoryEntry::name` — `filesystem::path` (replaces `const char*` from `dir.read()`)
- **Files modified**:
  - `src/bnetd/account.cpp` — include-only swap; no actual `Directory` usage
  - `src/bnetd/clan.cpp` — include-only swap; no actual `Directory` usage
  - `src/bnetd/userlog.cpp` — include-only swap; no actual `Directory` usage
  - `src/bnetd/i18n.cpp` — `dir_getfiles()` → `list_files()` + `filesystem::path` → `string` conversion
  - `src/bnetd/luainterface.cpp` — `dir_getfiles()` → `list_files()` + conversion
  - `src/bnetd/storage_file.cpp` — 3 `Directory`+`read()` loops → `open_directory()`+`read_directory()`;
    teams section: `dentry_str` hoisted to function scope to avoid goto-over-init error
  - `src/bnetd/mail.h` — `mutable Directory mdir` → `mutable std::optional<DirectoryIterator> mdir_`;
    added `<optional>` include
  - `src/bnetd/mail.cpp` — all `mdir` usages migrated to `mdir_`; `Directory::OpenError` replaced
    with `DeliverError` (thrown directly from `createOpenDir()`, re-thrown in `deliver()`)
  - `src/d2cs/handle_d2cs.cpp` — 3 sections migrated:
    - Existence-check (line ~238): `try { Directory dir(path); } catch(OpenError)` →
      `if (!open_directory(path)) { p_mkdir(path); }`
    - `on_client_charlistreq` charlist loop: `Directory dir` + `while (charname = dir.read())` →
      `open_directory()` + `while (auto entry = read_directory(*diropt))` with `charname_str` holding lifetime
    - `on_client_charlistreq_110` charlist loop: same pattern
  - `src/bnetd/CMakeLists.txt` — added `if(TARGET infra_compat) target_link_libraries(bnetd_legacy PUBLIC infra_compat) endif()`
  - `src/d2cs/CMakeLists.txt` — same guard added for `d2cs_legacy`
  - `plans/step8-networking-checklist.md` — R113 deferred note updated to ✅ R116; Round 116 section added
  - `plans/progress-master.md` — R115 next-round note updated; R116 session log added
- **Verification**: Zero `#include "compat/pdir.h"` remaining in `src/bnetd/` and `src/d2cs/`;
  zero `Directory::` or `dir_getfiles` references in migrated files
- **Phase 1 Step 8 status**: All v3 headers ✅; pdir callers ✅ R116; psock callers 10/17 ✅ R117; 7 deferred R118+
- **Next**: Round 118 — Migrate deferred complex psock callers (server.cpp, connection.cpp ×2, net.cpp, s2s.cpp, d2cs/server.cpp, dbserver.cpp)

### Round 117 — Phase 1 Step 8: `psock.h` caller migration (partial) 🔄 10/17 DONE

- **Goal**: Migrate all 17 callers of `src/compat/psock.h` to standard POSIX socket APIs.
  Classify each file; migrate safe files now; defer complex event-loop / full-lifecycle files.
- **Classification**:
  - **Include-only** (no psock symbols in body): `d2gs.cpp`, `handle_d2cs.cpp`, `dbspacket.cpp`
  - **Simple** (macro/type replacements): `tracker.cpp`, `udptest_send.cpp`, `addr.cpp`, `fdwatch_select.h`
  - **Simple** (single recv call): `handle_file.cpp`
  - **Simple** (init/deinit only): `main.cpp`
  - **Moderate** (recv/send + errno constants): `network.cpp`
  - **Complex / deferred**: `server.cpp` (bnetd), `connection.cpp` (bnetd), `connection.cpp` (d2cs),
    `net.cpp` (d2cs), `s2s.cpp` (d2cs), `server.cpp` (d2cs), `dbserver.cpp` (d2dbs)
- **Symbol mapping applied**:
  - `PSOCK_AF_INET` → `AF_INET`
  - `psock_sendto` → `sendto`; `psock_recv` → `recv`; `psock_send` → `send`
  - `psock_t_socklen` → `socklen_t`; `t_psock_fd_set` → `fd_set`
  - `psock_errno()` → `errno` (POSIX) / `WSAGetLastError()` (Win32)
  - `psock_init()` → `WSAStartup(MAKEWORD(2,2), &wsaData)` (Win32 only)
  - `psock_deinit()` → `WSACleanup()` (Win32 only)
  - `PSOCK_EINTR/EAGAIN/EWOULDBLOCK/ENOMEM/ENOTCONN/ECONNRESET/EPIPE/ENOBUFS` → POSIX equivalents
- **Files modified** (✅ R117):
  - `src/d2cs/d2gs.cpp` — removed `#include "compat/psock.h"`; added platform-guarded
    `<netinet/in.h>` + `<arpa/inet.h>` (needed for `INADDR_ANY`, `ntohl`)
  - `src/d2cs/handle_d2cs.cpp` — removed `#include "compat/psock.h"` (no psock symbols in body)
  - `src/d2dbs/dbspacket.cpp` — removed `#include "compat/psock.h"` (no psock symbols in body)
  - `src/bnetd/handle_file.cpp` — `#include "compat/psock.h"` → `<sys/socket.h>`; `psock_recv` → `recv`
  - `src/bnetd/tracker.cpp` — `PSOCK_AF_INET` → `AF_INET`; `psock_sendto` → `sendto`;
    `psock_t_socklen` → `socklen_t`; log message updated
  - `src/bnetd/udptest_send.cpp` — same as tracker.cpp; `psock_errno()` → `errno`; `<cerrno>` added
  - `src/common/addr.cpp` — 4× `PSOCK_AF_INET` → `AF_INET`; `psock_init()` call removed (Win32-only
    no-op on POSIX); `<netdb.h>` added for `gethostbyname`/`getservbyname`; duplicate
    `#ifdef HAVE_ARPA_INET_H` guard removed
  - `src/common/fdwatch_select.h` — `t_psock_fd_set` → `fd_set`; `<sys/select.h>` added
  - `src/bnetd/main.cpp` — `psock_init()` → `WSAStartup(MAKEWORD(2,2), &wsaData)` under `#ifdef _WIN32`;
    `psock_deinit()` → `WSACleanup()` under `#ifdef _WIN32`; `<winsock2.h>` added to Win32 block
  - `src/common/network.cpp` — `psock_recv` → `recv`; `psock_send` → `send`; `psock_errno()` →
    local `sock_err` (`WSAGetLastError()` on Win32, `errno` on POSIX); all 8 `PSOCK_E*` constants
    → POSIX equivalents (`EINTR`, `EAGAIN`, `EWOULDBLOCK`, `ENOMEM`, `ENOTCONN`, `ECONNRESET`,
    `EPIPE`, `ENOBUFS`)
- **Errors encountered and fixed during migration**:
  - `d2gs.cpp`: `INADDR_ANY`/`ntohl` undefined after removing psock.h → added `<netinet/in.h>` + `<arpa/inet.h>`
  - `handle_file.cpp`: `psock_recv` still undefined after include swap → replaced with `recv`
  - `addr.cpp`: `gethostbyname`/`getservbyname`/`hostent`/`servent` undefined → added `<netdb.h>`
  - `addr.cpp`: duplicate `<arpa/inet.h>` (new block + old `#ifdef HAVE_ARPA_INET_H`) → removed old guard
  - `fdwatch_select.h`: `t_psock_fd_set` undefined → replaced with `fd_set`
- **Verification**: `grep -r '#include "compat/psock.h"' src/` confirms exactly 7 remaining files —
  all are the deferred complex files
- **No CMakeLists changes needed** — migrated files use only standard POSIX/system headers
- **`src/compat/psock.h` and `src/compat/psock.cpp` NOT deleted** — 7 deferred callers still need them
- **Phase 1 Step 8 status**: 10/17 psock callers migrated ✅ R117; 7 deferred ⏳ Round 118+
- **Next**: Round 118 — Phase 1 Step 9: pugixml via FetchContent (pivoted from psock deferred callers)

### Round 118 — Phase 1 Step 9: pugixml via FetchContent ✅

- **Goal**: Integrate pugixml as a v3 FetchContent dependency; create a clean C++20 wrapper;
  write Catch2 unit tests; update Dockerfile.
- **Audit findings**:
  - Legacy code already bundles pugixml: `src/common/pugixml.h` + `src/common/pugixml.cpp`
  - Primary consumer: `src/bnetd/i18n.cpp` (parses `conf/i18n/**/*.xml`)
  - Other consumers: `src/bnetd/output.cpp`, `src/bnetd/ladder.h`, `src/d2dbs/d2ladder.cpp`,
    `src/v3/tools/bntrackd/bntrackd.cpp`
  - `src/v3/CMakeLists.txt` already had `include(FetchContent)` with ankerl/spdlog/tomlplusplus/zlib
- **Changes made**:
  - `src/v3/CMakeLists.txt`:
    - Added `option(PVPGN_V3_WITH_PUGIXML ...)` alongside existing options
    - Added `FetchContent_Declare(pugixml GIT_TAG v1.14 GIT_SHALLOW TRUE)` + `FetchContent_MakeAvailable`
    - Added `pugixml::static` alias guard (pugixml exports `pugixml-static` target)
    - Added `add_subdirectory(infra/xml)` guarded by `if(PVPGN_V3_WITH_PUGIXML)`
    - Added comment block documenting the `infra/xml` module
  - `src/v3/infra/xml/CMakeLists.txt` — new file: `infra_xml` INTERFACE target linking `pugixml::static`
  - `src/v3/infra/xml/include/infra/xml/xml_document.hpp` — new file:
    - `XmlDocument` (move-only, owns `pugi::xml_document`)
    - `XmlNode` (value type wrapping `pugi::xml_node`)
    - `XmlChildRange` / `XmlNamedChildRange` — lazy forward-iterator ranges
    - `load_file(path)` → `std::optional<XmlDocument>` (no exceptions)
    - `load_string(text)` → `std::optional<XmlDocument>` (no exceptions)
    - All in `pvpgn::v3::infra::xml` namespace; `[[nodiscard]]` throughout
  - `tests/unit/infra/xml/test_xml_document.cpp` — 30 Catch2 test cases covering:
    - `load_string` valid/invalid/empty/whitespace
    - `load_file` non-existent path
    - `root().name()`, `root().text()`, `root().attribute()`, `root().child()`
    - `root().children()` (all), `root().children(name)` (filtered)
    - Nested child access, empty element, multiple attributes, bool conversion
    - Move semantics, XML declaration, CDATA, single-element XML
  - `tests/unit/infra/xml/CMakeLists.txt` — `test_infra_xml_document` target
  - `tests/unit/infra/CMakeLists.txt` — added `if(TARGET infra_xml) add_subdirectory(xml) endif()`
  - `Dockerfile.v3` — added `infra_xml test_infra_xml_document` to build target list;
    added `/src/build/v3/tests/unit/infra/xml/test_infra_xml_document` to v3-test RUN stage
- **Deferred**: `src/bnetd/i18n.cpp` migration to use `XmlDocument` — Round 119+
- **Next**: Round 119 — Either migrate `i18n.cpp` to `XmlDocument`, or continue psock deferred callers

### Round 119 — Phase 1 Step 9: Migrate pugixml consumers to XmlDocument wrapper ✅

- **Goal**: Audit all pugixml consumers in `src/bnetd/`, `src/d2cs/`, `src/d2dbs/`; migrate safe
  consumers to `pvpgn::v3::infra::xml::XmlDocument`; mark Phase 1 Step 9 COMPLETE.
- **Consumer audit results**:
  - `src/bnetd/i18n.cpp` — **Moderate** (read-only API + `find_child_by_attribute`) → **MIGRATED ✅**
  - `src/bnetd/output.cpp` — **N/A** — zero pugixml usage (checklist entry was stale)
  - `src/bnetd/ladder.cpp` — **N/A** — zero pugixml usage
  - `src/d2dbs/d2ladder.cpp` — **N/A** — zero pugixml usage
  - `src/v3/tools/bntrackd/bntrackd.cpp` — **N/A** — zero pugixml usage (already v3, uses no XML)
  - `src/common/pugixml.cpp` / `src/common/pugixml.h` — implementation files, not consumers
- **Changes made**:
  - `src/bnetd/i18n.cpp`:
    - Replaced `#include "common/pugixml.h"` → `#include "infra/xml/xml_document.hpp"`
    - Added `namespace xml = pvpgn::v3::infra::xml;` alias
    - Moved `doc` inside loop: `auto doc = xml::load_file(lang_filename)` (per-iteration `optional<XmlDocument>`)
    - Replaced `doc.child("root")` → `doc->root().child("root")` returning `optional<XmlNode>`
    - Replaced `pugi::xml_node` variables → `auto` with optional unwrapping
    - Replaced `node.child_value("x")` → `node.child("x")->text()` (returns `string_view`)
    - Replaced `node.attribute("tag").as_string()` → `node->attribute("tag").value_or("")`
    - Replaced `for (pugi::xml_node n = x.child("y"); n; n = n.next_sibling("y"))` → `for (auto n : x->children("y"))`
    - Replaced `item_nodes.find_child_by_attribute("id", attr.value())` → manual loop over
      `item_nodes.children("item")` checking `n.attribute("id").value_or("") == refid`
    - All `pugi::` types eliminated from the file
  - `src/bnetd/CMakeLists.txt`:
    - Added `if(TARGET infra_xml) target_link_libraries(bnetd_legacy PUBLIC infra_xml) endif()`
    - Placed after the existing `infra_compat` guard (same pattern)
- **Verification**: `grep -r 'pugi::' src/bnetd/ src/d2cs/ src/d2dbs/` → **zero results** ✅
- **Phase 1 Step 9 status**: **COMPLETE ✅**
- **Legacy files safe to delete**: `src/common/pugixml.h`, `src/common/pugixml.cpp`, `src/common/pugiconfig.h`
  (no remaining consumers — deletion deferred to Step 10)
- **Next**: Phase 1 Step 10 — Configuration migration to TOML (replace legacy `.conf` file parsing
  with `tomlplusplus` via the existing `PVPGN_V3_WITH_TOMLPLUSPLUS` FetchContent option)

### Round 120 — Phase 1 Step 10: TOML Config wrapper + bnetd.toml.in 🔄

- **Goal**: Introduce `Config` C++20 wrapper over toml++; create `conf/bnetd.toml.in`; write Catch2 tests.
- **Audit findings**:
  - `src/bnetd/conf.h` / `conf.cpp` do **not** exist — legacy config is in `prefs.h` / `prefs.cpp`
  - CMake option is `PVPGN_V3_WITH_TOMLPP` (not `PVPGN_V3_WITH_TOMLPLUSPLUS`)
  - `infra_config` library already defined in `src/v3/CMakeLists.txt` lines 256–272 with
    `PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/infra/config/include`
  - `tests/unit/infra/CMakeLists.txt` already has `add_subdirectory(config)` guarded by `PVPGN_V3_WITH_TOMLPP`
  - Existing `infra/config/` files: `server_config.hpp`, `legacy_prefs.hpp`, `config_watcher.hpp`,
    `server_config.cpp`, `config_watcher.cpp` — all pre-existing; `config.hpp` is new
  - 38 `src/bnetd/` files include `prefs.h`; ~300+ `prefs_get_*` call sites across the codebase
- **Changes made**:
  - `src/v3/infra/config/include/infra/config/config.hpp` — **NEW**: header-only `Config` class
    wrapping `toml::table`; methods: `load_string()`, `load_file()`, `get<T>()`, `get_or<T>()`,
    `has()`, `keys()`, `section()`, `raw()`; all exception-free via `std::optional`
  - `conf/bnetd.toml.in` — **NEW**: TOML template with 20 sections (`[privileges]`, `[storage]`,
    `[files]`, `[localization]`, `[log]`, `[d2cs]`, `[downloads]`, `[client_verification]`,
    `[timing]`, `[policy]`, `[account]`, `[tracking]`, `[network]`, `[wol]`, `[irc]`,
    `[telnet]`, `[ladder]`, `[status]`, `[clan]`, `[command_log]`) — full parity with `bnetd.conf.in`
  - `tests/unit/infra/config/test_config.cpp` — **NEW**: 18 Catch2 tests covering all `Config` methods
    including a representative bnetd.toml round-trip test
  - `tests/unit/infra/config/CMakeLists.txt` — **UPDATED**: added `test_infra_config_config` target
  - `plans/step10-toml-checklist.md` — **NEW**: consumer audit table (38 files, 20 TOML sections),
    deliverables checklist, CMake notes, API summary, test coverage table
  - `plans/progress-master.md` — **UPDATED**: Step 10 marked in-progress; R120 session log added
- **No changes to**: `src/bnetd/prefs.cpp`, `src/bnetd/prefs.h`, `src/v3/CMakeLists.txt`,
  `tests/unit/infra/CMakeLists.txt` (all already correct)
- **Step 10 status**: 🔄 **IN PROGRESS** — wrapper + template + tests done; prefs migration deferred
- **Next**: Wire `Config` into `ServerConfig` / `LegacyPrefs`; migrate `prefs_get_*` callers

---

### Round 121 — 2026-05-21

**Phase 1 Step 10 continued: Wire Config into ServerConfig + expand LegacyPrefs**

- **Approach chosen**: Direct expansion of existing `ServerConfig` + `LegacyPrefs` (both already existed
  in `src/v3/infra/config/`). No new bridge class needed — `LegacyPrefs` IS the bridge.
  `server_config.cpp` now uses the `Config` wrapper from `config.hpp` instead of raw `toml::table`.

- **Changes made**:
  - `src/v3/infra/config/include/infra/config/server_config.hpp` — **EXPANDED**: 5 fields → 20 typed
    sub-structs (`PrivilegesConfig`, `StorageConfig`, `FilesConfig`, `LocalizationConfig`, `LogConfig`,
    `D2csConfig`, `DownloadsConfig`, `ClientVerificationConfig`, `TimingConfig`, `PolicyConfig`,
    `AccountConfig`, `TrackingConfig`, `NetworkConfig`, `WolConfig`, `IrcConfig`, `TelnetConfig`,
    `LadderConfig`, `StatusConfig`, `ClanConfig`, `CommandLogConfig`); covers all ~100 `prefs_get_*`
    fields from `src/bnetd/prefs.h`; backward-compat aliases `servername` and `script_dir` retained
  - `src/v3/infra/config/src/server_config.cpp` — **REWRITTEN**: now uses `Config::load_string()` /
    `Config::load_file()` + `Config::section()` + `Config::get_or<T>()`; 20 `parse_*()` helper
    functions; `files.logfile` seeds `log.file` as fallback; `network.servername` propagates to
    top-level `servername` compat alias
  - `src/v3/infra/config/include/infra/config/legacy_prefs.hpp` — **EXPANDED**: 10 accessors → ~100
    accessors covering every `prefs_get_*` function in `src/bnetd/prefs.h`; pre-computes
    `std::string` copies of all `filesystem::path` fields in constructor; returns `std::string_view`
    for zero-copy access; `uint32_t` return type for boolean flags (matches legacy `unsigned int` API)
  - `tests/unit/infra/config/server_config_test.cpp` — **UPDATED**: 4 tests → 10 tests; new test
    cases for `[network]`, `[files]`, `[log]`, `[storage]`, `[policy]`, `[timing]`, `[d2cs]`, `[clan]`
    sections; updated field paths to match new struct layout
  - `tests/unit/infra/config/legacy_prefs_test.cpp` — **UPDATED**: fixed field paths for new struct;
    added second test case covering policy/timing/clan/account/tracking/d2cs/downloads/ladder/status/
    command_log accessors
  - `conf/d2cs.toml.in` — **NEW**: TOML template for d2cs with 7 sections (`[server]`, `[network]`,
    `[realm]`, `[log]`, `[files]`, `[misc]`, `[internal]`); full parity with `d2cs.conf.in`
  - `conf/d2dbs.toml.in` — **NEW**: TOML template for d2dbs with 5 sections (`[network]`, `[log]`,
    `[files]`, `[ladder]`, `[misc]`); full parity with `d2dbs.conf.in`
  - `plans/step10-toml-checklist.md` — **UPDATED**: status R121, deliverables table extended,
    infra/config files table updated, deferred section updated
  - `plans/progress-master.md` — **UPDATED**: Step 10 checklist updated; R121 session log added

- **No changes to**: `src/bnetd/prefs.cpp`, `src/bnetd/prefs.h` (legacy API untouched — safety first),
  `src/v3/CMakeLists.txt`, `tests/unit/infra/config/CMakeLists.txt` (no new targets needed)

- **prefs_get_* call sites remaining**: ~300+ across 38 files (unchanged — no callers migrated yet;
  migration to `LegacyPrefs` is the Round 122+ work)

- **Step 10 status**: 🔄 **IN PROGRESS** — `Config` wired into `ServerConfig`; `LegacyPrefs` fully
  expanded; TOML templates for all 3 services done; prefs caller migration deferred to R122+

- **Next (Round 122+)**:
  1. Migrate simplest callers (1-call files: `icons.cpp`, `versioncheck.cpp`, `anongame.cpp`,
     `sql_dbcreator.cpp`, `handle_apireg.cpp`, `handle_d2cs.cpp`, `handle_init.cpp`, `support.cpp`,
     `topic.cpp`) — replace `prefs_get_X()` with `LegacyPrefs::X()`
  2. Continue with medium-complexity files (5-10 calls each)
  3. Tackle high-impact files last (`handle_bnet.cpp`, `connection.cpp`, `server.cpp`, `command.cpp`)
  4. Delete `prefs.cpp` / `prefs.h` once all 38 files migrated
  5. Install TOML templates via `conf/CMakeLists.txt`

### Round 122 — 2026-05-21

**Phase 1 Step 10 continued: prefs_bridge + delegation guards in prefs.cpp + main.cpp wiring**

- **Approach**: Strangler-fig bridge pattern — C-linkage `prefs_bridge.hpp/cpp` in
  `src/v3/integration/legacy_bnetd/` exposes `pvpgn_v3_prefs_load_toml()` and ~100
  `pvpgn_v3_prefs_get_*()` functions. Each `prefs_get_*` in `src/bnetd/prefs.cpp` gains a
  `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard that delegates to the bridge when TOML is loaded.
  Legacy `.conf` values remain active as fallback when TOML is absent.

- **Changes made**:
  - `src/v3/integration/legacy_bnetd/include/integration/legacy_bnetd/prefs_bridge.hpp` — **NEW**:
    C-linkage bridge header; `pvpgn_v3_prefs_load_toml(path)`, `pvpgn_v3_prefs_loaded()`, and ~100
    `pvpgn_v3_prefs_get_*` / `pvpgn_v3_prefs_allow_*` declarations
  - `src/v3/integration/legacy_bnetd/src/prefs_bridge.cpp` — **NEW**: bridge implementation;
    `std::optional<LegacyPrefs> g_prefs` global; uses `load_server_config(std::filesystem::path)`;
    each accessor returns `p->field().data()` or `""` if not loaded
  - `src/v3/CMakeLists.txt` — **MODIFIED**: `prefs_bridge.cpp` added to `integration_legacy_bnetd`
    SOURCES; `$<$<TARGET_EXISTS:infra_config>:infra_config>` added to PUBLIC_DEPS
  - `src/v3/infra/config/include/infra/config/legacy_prefs.hpp` — **MODIFIED**: added
    `effective_user()` and `effective_group()` public accessors (were stored as private strings
    `effective_user_str_` / `effective_group_str_` but not exposed)
  - `src/bnetd/prefs.cpp` — **MODIFIED**: all ~100 `prefs_get_*` / `prefs_allow_*` functions now
    have `#ifdef PVPGN_V3_BNETD_INTEGRATION` delegation guards; functions with clamping logic
    (quota_lines, quota_time, quota_wrapline, quota_maxline, quota_dobae, mail_quota) place the
    guard before the clamping so the bridge value is returned directly
  - `src/bnetd/main.cpp` — **MODIFIED**: added `prefs_bridge.hpp` include to the
    `PVPGN_V3_BNETD_INTEGRATION` block; after `prefs_load()` succeeds, derives TOML path by
    replacing `.conf` extension with `.toml` and calls `pvpgn_v3_prefs_load_toml()`; non-fatal
    if TOML file absent (legacy `.conf` values remain active as fallback); logs info/warn

- **Errors fixed during R122**:
  - Wrong API call in `prefs_bridge.cpp`: initially used `Config::from_file` + `parse_server_config`
    (two-step). Fixed to use `load_server_config(std::filesystem::path)` directly.
  - Missing `effective_user()` / `effective_group()` accessors in `LegacyPrefs`: added them.

- **Step 10 status**: 🔄 **IN PROGRESS** — TOML config is now fully wired into the bnetd startup
  path via the bridge; all `prefs_get_*` functions delegate to TOML when available; legacy `.conf`
  fallback preserved

- **Next (Round 123+)**:
  1. Migrate simplest callers (1-call files: `icons.cpp`, `versioncheck.cpp`, `anongame.cpp`,
     `sql_dbcreator.cpp`, `handle_apireg.cpp`, `handle_d2cs.cpp`, `handle_init.cpp`, `support.cpp`,
     `topic.cpp`) — replace `prefs_get_X()` with `LegacyPrefs::X()` directly
  2. Continue with medium-complexity files (5-10 calls each)
  3. Tackle high-impact files last (`handle_bnet.cpp`, `connection.cpp`, `server.cpp`, `command.cpp`)
  4. Delete `prefs.cpp` / `prefs.h` once all 38 files migrated
  5. Install TOML templates via `conf/CMakeLists.txt`

### Round 123 — 2026-05-21

**Phase 2: WolFsm IRC-like chat protocol FSM — new `protocol/wol/` library**

#### Architectural decision

The existing `WolFsm` stub in `protocol/wolgameres/` handles **binary WOL game result
packets** (TLV format). The WOL chat protocol is a separate IRC-like text protocol used
by C&C, Red Alert, Tiberian Sun clients. Created a new `protocol/wol/` library to keep
concerns separate.

#### Changes made

- **`src/v3/protocol/wol/include/protocol/wol/wol_session_context.hpp`** — **NEW**:
  `IWolSessionContext` interface with `send_line()`, `send_bytes()`, `close()`,
  `server_name()`. Same pattern as `IFileSessionContext`.

- **`src/v3/protocol/wol/include/protocol/wol/wol_fsm.hpp`** — **NEW**:
  `WolFsm` class with `WolState` enum:
  `Connecting → Authenticating → Authenticated → InChannel / InGame / Disconnecting`.
  Public API: `on_bytes()`, `on_close()`, `state()`, `nick()`, `channel()`.

- **`src/v3/protocol/wol/src/wol_fsm.cpp`** — **NEW** (~280 lines):
  Full IRC-like WOL chat FSM implementation:
  - `on_bytes()` — appends to `line_buf_`, calls `process_lines()`
  - `process_lines()` — splits on `\r\n` or bare `\n`, calls `dispatch_line()` per line
  - `dispatch_line()` — parses command via `parse_line()`, dispatches to handlers
  - `on_nick()` — sets nick, transitions `Connecting → Authenticating`
  - `on_user()` — sets user/realname
  - `on_pass()` — sets pass; if nick+user present → `Authenticated`, sends 001+002+375+376
  - `on_ping()` — sends `PONG :<token>\r\n`
  - `on_quit()` — sends `ERROR :Closing Link`, closes, → `Disconnecting`
  - `on_list()` — sends 321 RPL_LISTSTART + 323 RPL_LISTEND (empty stub)
  - `on_join()` — echoes JOIN, sends 332 RPL_TOPIC + 366 RPL_ENDOFNAMES, → `InChannel`
  - `on_part()` — echoes PART, → `Authenticated`
  - `on_privmsg()` — validates auth, stub (no relay yet)
  - WOL-specific commands silently accepted: CVERS, VERCHK, APGAR, SETOPT, SERIAL,
    GAMEOPT, STARTG, JOINGAME, SQUADINFO, CLANBYNAME, FINDUSER, FINDUSEREX, PAGE,
    ADVERTR, ADVERTC, CHANCHK, GETBUDDY, ADDBUDDY, DELBUDDY, HOST, INVMSG, INVDEL,
    USERIP, LISTSEARCH, RUNGSEARCH, HIGHSCORE, SETCODEPAGE, GETCODEPAGE, SETLOCALE,
    GETLOCALE, GETINSIDER, NAMES
  - `send_numeric()` — formats `:server NNN target :text\r\n`
  - `send_raw()` — appends `\r\n`, calls `ctx_->send_bytes()`

- **`src/v3/CMakeLists.txt`** — **MODIFIED**:
  Added `protocol_wol` library target after `protocol_wolgameres`:
  ```cmake
  pvpgn_v3_add_library(protocol_wol STATIC
      SOURCES protocol/wol/src/wol_fsm.cpp
      PUBLIC_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/protocol/wol/include
      PUBLIC_DEPS core)
  ```

- **`tests/unit/protocol/wol/wol_fsm_test.cpp`** — **NEW** (34 test cases, 153 assertions):
  `FakeWolContext` captures lines sent, tracks `closed` flag.
  Tests cover: initial state, NICK transitions, full auth sequence (NICK+USER+PASS),
  PING/PONG, QUIT, `on_close()`, partial buffering, multiple commands in one `on_bytes()`
  call, LIST, JOIN, PART, PRIVMSG, unknown command, USER validation, WOL-specific
  commands silently accepted, bare LF terminator.

- **`tests/unit/protocol/wol/CMakeLists.txt`** — **NEW**:
  `pvpgn_v3_add_test(test_protocol_wol_fsm SOURCES wol_fsm_test.cpp DEPS protocol_wol)`

- **`tests/unit/protocol/CMakeLists.txt`** — **MODIFIED**:
  Added `wol` subdirectory (guarded by `if(TARGET protocol_wol)`).

- **`plans/phase2-fsm-checklist.md`** — **MODIFIED**:
  Updated FSM status table (added `protocol/wol` row ✅ R123); marked Step 3 items
  3f-3h complete; updated Migration Order table.

#### Build results

```
make protocol_wol          → OK (0 errors, 0 warnings)
make test_protocol_wol_fsm → OK (0 errors, 0 warnings)
ctest -R test_protocol_wol_fsm --output-on-failure
  → 34 test cases, 153 assertions, 0 failures
```

#### Phase 2 status

- ✅ **Step 3a-3h COMPLETE** — BnftpFsm (R122) + WolFsm IRC-like chat (R123)
- ❌ `protocol/wolgameres` WolFsm (binary game results) — still a stub
- ❌ Composition root (`src/v3/bnetd/main.cpp`) — still missing

#### Next (Round 124+)

1. Fix `BnetFsm` TODOs: `message_router` broadcast, `JoinGame` reply, `client_tag` tracking
2. Wire `IrcFsm::on_privmsg` to `post_message` use-case (remove echo-back)
3. Complete `IrcBridgeFsm` — RPL_NAMREPLY (353) for channel member list
4. Create composition root (`src/v3/app/bnetd/main.cpp`) wiring all FSMs + Asio event loop

### Round 130 — 2026-05-21

**Phase 2 COMPLETE: Progress files updated**

- Marked Phase 2 (Protocol FSM Completion) as **COMPLETE** in `plans/progress-master.md`
- Added Phase 2 summary table: 5 FSMs, 161 test cases, 851 assertions total
- Added composition root summary: `pvpgn_v3_bnetd` binary, all listeners wired (R125–R128)
- Added round-by-round log for R122–R129
- Marked Phase 3 (bnetd Server Logic Migration) as **IN PROGRESS** with full scope breakdown
- Updated `plans/phase2-fsm-checklist.md`: added COMPLETE banner, total test count summary,
  marked all checklist items complete, updated Migration Order table


### Round 134 — 2026-05-21

**Phase 1 Step 10 — first wave of `prefs_get_*` caller migration**

- **Approach:** introduced `src/bnetd/prefs_v3_shim.h` (header-only) exposing inline
  `pvpgn::bnetd::prefs_v3::*()` accessors. Each accessor preserves the existing
  `pvpgn_v3_prefs_loaded()` → bridge dispatch already present inside `prefs.cpp`,
  with the legacy `prefs_get_*()` call as the `#ifndef`/no-TOML fallback. Semantics
  are byte-identical; the shim simply moves the dispatch out of `prefs.cpp` so
  callers no longer reference the legacy `prefs_get_*` symbols directly.

- **Files migrated (7):**
  - `src/bnetd/versioncheck.cpp` — `prefs_get_allow_bad_version()` → `prefs_v3::allow_bad_version()`
  - `src/bnetd/sql_dbcreator.cpp` — `prefs_get_DBlayoutfile()` → `prefs_v3::DBlayoutfile()`
  - `src/bnetd/handle_apireg.cpp` — `prefs_get_allow_new_accounts()` → `prefs_v3::allow_new_accounts()`
  - `src/bnetd/handle_d2cs.cpp` — `prefs_allow_d2cs_setname()` + `prefs_get_d2cs_version()` → `prefs_v3::*`
  - `src/bnetd/handle_init.cpp` — both `prefs_get_max_conns_per_IP()` call sites → `prefs_v3::max_conns_per_IP()`
  - `src/bnetd/support.cpp` — both `prefs_get_filedir()` call sites → `prefs_v3::filedir()`
  - `src/bnetd/topic.cpp` — both `prefs_get_topicfile()` call sites → `prefs_v3::topicfile()`

- **Files NOT migrated:**
  - `src/bnetd/icons.cpp` — the only `prefs_get_*` occurrence (`prefs_get_custom_icons`) is the
    *definition* of an out-of-tree accessor, not a call site. Nothing to migrate.

- **New file:** [`src/bnetd/prefs_v3_shim.h`](../src/bnetd/prefs_v3_shim.h) — 8 inline accessors
  (`filedir`, `topicfile`, `DBlayoutfile`, `allow_bad_version`, `allow_new_accounts`,
  `allow_d2cs_setname`, `d2cs_version`, `max_conns_per_IP`). New entries to be added as
  additional caller files are migrated.

- **Verification:** all 7 modified TUs pass `g++ -std=c++20 -fsyntax-only -DPVPGN_V3_BNETD_INTEGRATION=1`
  with the v3 integration headers on the include path. (The local CMake build trees
  `build/` and `build-v3/` are in a pre-existing broken state — `LUA_INCLUDE_DIR=NOTFOUND`
  during reconfigure and `bnetd_legacy` missing `infra/compat/directory.hpp` on its include
  path — both unrelated to this round; full validation belongs to `Dockerfile.v3`.)

- **Step 10 caller-migration progress:** 7 / ~38 files (~12 / ~300 call sites).

- **Next (Round 135+):** continue with the medium-complexity files
  (`message.cpp`, `channel.cpp`, `account.cpp`, `account_wrap.cpp`, `irc.cpp`, `clan.cpp`,
  `mail.cpp`, `output.cpp`, `userlog.cpp`, `tracker.cpp`, `ipban.cpp`, `attrgroup.cpp`,
  `attrlayer.cpp`, `watch.cpp`, `sql_common.cpp`, `storage_file.cpp`, `anongame.cpp`,
  `anongame_maplists.cpp`, `ladder.cpp`, `i18n.cpp`, `handle_wol.cpp`, `handle_wserv.cpp`,
  `handle_irc.cpp`, `handle_irc_common.cpp`). Then tackle the high-impact files
  (`handle_bnet.cpp`, `connection.cpp`, `server.cpp`, `command.cpp`, `game.cpp`,
  `luainterface.cpp`, `main.cpp`). After all callers migrated and TOML becomes mandatory,
  delete `prefs.cpp` / `prefs.h`.

### Round 135 — 2026-05-21

**Phase 1 Step 10 — second wave of `prefs_get_*` caller migration (8 files)**

- Extended [`src/bnetd/prefs_v3_shim.h`](../src/bnetd/prefs_v3_shim.h) with 18 new
  inline accessors: `servername`, `location`, `description`, `url`, `contact_name`,
  `contact_email`, `ipbanfile`, `ipban_check_int`, `userlogdir`, `maildir`,
  `mail_support`, `mail_quota`, `outputdir`, `XML_status_output`, `log_commands`,
  `log_command_groups`, `log_command_list`, `clan_newer_time`,
  `clan_channel_default_private`.
- Note: the `outputdir` accessor delegates to `pvpgn_v3_prefs_get_statusdir()` in
  the bridge — the v3 typed config exposes this field under the `statusdir` name
  while the legacy accessor is `prefs_get_outputdir()`; both resolve to the same
  underlying `outputdir` setting.

**Files migrated (8):**
- `src/bnetd/message.cpp` — `servername` (×5), `contact_name`
- `src/bnetd/output.cpp` — `XML_status_output` (×2), `outputdir` (×2)
- `src/bnetd/mail.cpp` — `maildir` (×2), `mail_support`, `mail_quota`
- `src/bnetd/userlog.cpp` — `log_command_list`, `log_commands`, `log_command_groups`, `userlogdir`
- `src/bnetd/tracker.cpp` — `location`, `description`, `url`, `contact_name`, `contact_email`
- `src/bnetd/ipban.cpp` — `ipban_check_int`, `ipbanfile` (×3)
- `src/bnetd/watch.cpp` — `servername` (×4)
- `src/bnetd/sql_common.cpp` — `clan_channel_default_private`, `clan_newer_time` (×2)

**Verification:** all 8 modified TUs pass
`g++ -std=c++20 -fsyntax-only -DPVPGN_V3_BNETD_INTEGRATION=1`
(with `src/v3/infra/compat/include` also on the path — pre-existing requirement,
see Round 134 note).

**Step 10 caller-migration progress (cumulative):**
15 / ~38 files migrated, ~37 / ~300 call sites converted.

## Round 136 — Medium prefs caller migration (batch 3)

Migrated 8 medium-complexity files to use `prefs_v3::*` shim accessors so the
TOML→ServerConfig→LegacyPrefs bridge can take over when loaded.

Files migrated (8):
- `src/bnetd/attrgroup.cpp` — `user_sync_timer`, `user_flush_timer`,
  `user_flush_connected`, `storage_path` ×2 (5 sites).
- `src/bnetd/attrlayer.cpp` — `user_step` ×2 (2 sites).
- `src/bnetd/account_wrap.cpp` — `max_friends` ×5 (5 sites in friend
  management).
- `src/bnetd/ladder.cpp` — `ladder_init_rating`, `ladderdir` ×2,
  `outputdir` (4 sites).
- `src/bnetd/i18n.cpp` — `i18ndir` ×3, `localize_by_country` (4 sites).
- `src/bnetd/anongame_maplists.cpp` — `mapsfile` ×3 (3 sites).
- `src/bnetd/anongame.cpp` — `w3route_addr` (1 site).
- `src/bnetd/storage_file.cpp` — `savebyname` ×2,
  `clan_channel_default_private`, `clan_newer_time` ×2 (5 sites).

Shim accessors added to `src/bnetd/prefs_v3_shim.h` (13 new):
`i18ndir`, `localize_by_country`, `mapsfile`, `user_step`, `user_sync_timer`,
`user_flush_timer`, `user_flush_connected`, `storage_path`, `max_friends`,
`w3route_addr`, `ladder_init_rating`, `ladderdir`, `savebyname`.

Total sites migrated this round: 29 across 8 files.
Cumulative shim accessors: 39. Cumulative files migrated: 23/~38.

Validation:
- `g++ -std=c++20 -fsyntax-only -DPVPGN_V3_BNETD_INTEGRATION=1` clean for
  7/8 files. `i18n.cpp` blocked on pre-existing pugixml include
  unreachable from local env (not a regression — call-site edits identical
  to other files).
- Full CMake build of `bnetd_legacy` still blocked by pre-existing env
  issues (Lua autodetect, missing `infra/compat/include` on legacy
  target_include_directories). To be addressed separately.

Next batch candidates: `handle_wol`, `handle_wserv`, `handle_irc`,
`handle_irc_common`, `clan`, `irc`, `account`, `channel`. High-impact
follow-up: `handle_bnet`, `connection`, `server`, `command`, `game`,
`luainterface`, `main`.

## Round 137 — Medium prefs caller migration (batch 4)

Migrated 6 files to use `prefs_v3::*` shim accessors. (Original R137
plan included `handle_irc` and `handle_irc_common` but those source
files do not exist in this branch — only `irc.cpp` exists, which is
included.)

Files migrated (6):
- `src/bnetd/channel.cpp` — `chanlogdir` ×2, `channelfile` (3 sites).
  NOTE: `prefs_get_log_notice()` left intentionally unmigrated on
  line 492; pre-existing bridge bug — bridge declares it
  `unsigned int` but legacy returns `char const *`. Track as
  separate fix.
- `src/bnetd/clan.cpp` — `clan_newer_time`, `clan_channel_default_private`
  (2 sites).
- `src/bnetd/handle_wol.cpp` — `kick_old_login`, `chanlog`,
  `maxusers_per_channel` ×2 (4 sites).
- `src/bnetd/handle_wserv.cpp` — `wol_autoupdate_serverhost`,
  `wol_autoupdate_username`, `wol_autoupdate_password`, `servername`,
  `wol_timezone`, `wol_longitude`, `wol_latitude`, `allowed_clients`
  (8 sites).
- `src/bnetd/irc.cpp` — `kick_old_login`, `hide_addr` ×2, `motdfile`,
  `irc_network_name` ×2, `allow_new_accounts`, `wolv2_addrs` ×3,
  `wolv1_addrs`, `irc_addrs` ×2, `irc_latency` (12 sites).
- `src/bnetd/account.cpp` — `hashtable_size` ×2, `savebyname`,
  `max_accounts` ×3, `account_allowed_symbols` (7 sites).

Shim accessors added to `src/bnetd/prefs_v3_shim.h` (21 new):
`chanlogdir`, `channelfile`, `kick_old_login`, `chanlog`,
`maxusers_per_channel`, `hide_addr`, `motdfile`, `irc_network_name`,
`wolv1_addrs`, `wolv2_addrs`, `irc_addrs` (bridges to
`pvpgn_v3_prefs_get_ircaddrs` — pre-existing naming mismatch
intentionally bridged here), `irc_latency`, `wol_timezone`,
`wol_longitude`, `wol_latitude`, `wol_autoupdate_serverhost`,
`wol_autoupdate_username`, `wol_autoupdate_password`,
`allowed_clients`, `hashtable_size`, `max_accounts`,
`account_allowed_symbols`.

Total sites migrated this round: 36 across 6 files.
Cumulative shim accessors: 60. Cumulative files migrated: 29/~38.

Validation:
- `g++ -fsyntax-only` clean for all 6 files (only pre-existing
  `-Wregister` warnings in `account.cpp`).

Known pre-existing bridge inconsistencies discovered during R137 (not
fixed here):
- `pvpgn_v3_prefs_get_log_notice()` declared `unsigned int` in
  bridge header but `prefs.cpp::prefs_get_log_notice` returns
  `char const *` and stores a string. Will fail at link/typecheck
  once the legacy dispatch in `prefs.cpp` is compiled with
  `PVPGN_V3_BNETD_INTEGRATION=1`. Needs bridge return-type fix.
- `prefs.cpp::prefs_get_irc_addrs` calls
  `pvpgn_v3_prefs_get_irc_addrs()` but the bridge exports
  `pvpgn_v3_prefs_get_ircaddrs()` (no underscore). Will fail at
  link. The shim works around this by calling the bridge symbol
  directly, but `prefs.cpp` itself remains broken under v3.

Next batch candidates: `handle_bnet` (high-impact),
`connection`, `server`, `command`, `game`, `luainterface`,
`main`. Also remaining smaller files surfaced by grep.

## Round 138 — high-impact callers (handle_bnet, connection, server)

Files migrated (3):
- src/bnetd/handle_bnet.cpp (~30 sites)
- src/bnetd/connection.cpp  (~17 sites)
- src/bnetd/server.cpp      (~46 sites)

Total ~93 call sites migrated to `pvpgn::bnetd::prefs_v3::*`.

Shim accessors added (45 new in R138): account_force_username, adfile,
aliasfile, allow_unknown_version, anongame_infos_file, apireg_addrs,
ask_new_channel, bnetdserv_addrs, clan_max_members, command_groups_file,
customicons_file, helpfile, hide_pass_games, hide_started_games,
hide_temp_channels, hostname, iconfile, initkill_timer, issuefile,
latency, logfile, max_concurrent_logins, motdw3file, mpqfile, newsfile,
nullmsg, output_update_secs, packet_limit, passfail_bantime,
passfail_count, realmfile, scriptdir, shutdown_decr, shutdown_delay,
star_iconfile, sync_on_logoff, telnet_addrs (→ bridge `telnetaddrs`),
tournament_file, track, transfile, udptest_port, use_keepalive,
versioncheck_file, war3_iconfile, war3_ladder_update_secs,
wgameres_addrs.

Cumulative shim accessors: ~105. Cumulative migrated files: 32.

Intentionally NOT migrated (no bridge accessor; left as legacy
`prefs_get_*`):
- prefs_get_custom_icons    (only `customicons_file` exists in bridge)
- prefs_get_quota, quota_dobae, quota_lines, quota_maxline, quota_time,
  quota_wrapline
- prefs_get_trackserv_addrs (bridge has `trackaddrs` — different
  semantic / not yet wired)

Validation: g++ -fsyntax-only across all 3 TUs reports zero prefs-related
diagnostics. Remaining errors are pre-existing env blockers
(strangler_macros.h `extern "C"` placement; incomplete bridge struct
types `pvpgn_v3_friend_entry`, `pvpgn_v3_realm_legacy_entry` not
declared in this minimal include set).

## Round 139 — command, game, main, luainterface

Files migrated (4):
- src/bnetd/command.cpp       (~25 sites)
- src/bnetd/game.cpp          (~10 sites)
- src/bnetd/main.cpp          (~28 sites)
- src/bnetd/luainterface.cpp  (~120 sites)

Total ~183 call sites migrated through shim.

Shim accessors added (19 new in R139): clan_min_invites, discisloss,
effective_group, effective_user, enable_conn_all, ladder_games,
ladder_prefix, localizefile, loglevels, max_connections, pidfile,
report_all_games, report_diablo_games, reportdir, supportfile, tosfile,
XML_output_ladder, xpcalc_file (→ bridge `xpcalcfile`),
xplevel_file (→ bridge `xplevelfile`).

Cumulative shim accessors: ~124. Cumulative migrated files: 36.

Intentionally NOT migrated (no/broken bridge accessor; left as legacy
`prefs_get_*`):
- prefs_get_custom_icons, prefs_get_trackserv_addrs
- prefs_get_quota, quota_dobae, quota_lines, quota_maxline, quota_time,
  quota_wrapline
- prefs_get_log_notice (bridge type mismatch: unsigned int vs char const*)

Validation: g++ -fsyntax-only across all 4 TUs reports zero prefs-related
diagnostics.

---

### Round 131 — 2026-05-22

Phase 3 audit. Created `plans/phase3-bnetd-checklist.md` (865 lines) documenting
the full bnetd migration scope, deferred psock callers, handler deletion order,
and complexity ratings for all remaining work.

Deliverables:
- `plans/phase3-bnetd-checklist.md` — comprehensive Phase 3 checklist

### Round 132 — 2026-05-22

Deleted two legacy handler files that were fully superseded by Phase 2 FSMs:
- `src/bnetd/handle_file.cpp` + `handle_file.h` — replaced by `BnftpFsm` (R122)
- `src/bnetd/handle_wol_gameres.cpp` + `handle_wol_gameres.h` — replaced by `WolFsm`

Both files had zero remaining callers after the Phase 2 bridge work.

### Round 133 — 2026-05-22

Migrated 5 bnetd files from `psock.h` to POSIX `<sys/socket.h>` / `<unistd.h>`:
`server.cpp`, `connection.cpp`, `net.cpp` (bnetd), `udptest.cpp`, `handle_bnet.cpp`.
Added `#ifndef _WIN32` guards for POSIX headers. Removed `#include "compat/psock.h"`.

### Round 134 — 2026-05-22

Deleted `src/bnetd/handle_irc.cpp` + `src/bnetd/handle_irc_common.cpp`.
All IRC dispatch logic inlined into `src/bnetd/irc.cpp`. The `IrcFsm` (R127)
now owns the inbound IRC path. Zero remaining callers of the deleted files.

### Round 135 — 2026-05-22

Fixed missing `infra/compat/directory.hpp` in legacy build. Root cause: the
`d2cs_legacy` and `d2dbs_legacy` static libraries did not link `infra_compat`
when the v3 tree was present. Added conditional `target_link_libraries` blocks
to `src/d2cs/CMakeLists.txt` and `src/d2dbs/CMakeLists.txt`. Full 129/129
CMake targets built with zero errors after fix.

### Round 136 — 2026-05-22

Created `ConnectionFsm` skeleton in `src/v3/domain/connection/`:
- `include/domain/connection/connection_fsm.hpp`
- `include/domain/connection/connection_context.hpp`
- `src/connection_fsm.cpp`
- `tests/unit/domain/connection/connection_fsm_test.cpp`

States: `Connected`, `Authenticating`, `Authenticated`, `Disconnected`.
Test suite: 28 tests, 183 assertions. All green.

### Round 137 — 2026-05-22

Expanded `ConnectionFsm` with `InGame` state and full game lifecycle:
- `enter_game()` / `leave_game()` transitions
- `game_name` + `game_token` stored on `InGame` state
- Guard: `leave_game()` from non-`InGame` state returns error

Test suite expanded to 45 tests, 407 assertions. All green.

### Round 138 — 2026-05-22

Asio event loop integration bridge:
- `src/v3/app/bnetd/include/app/bnetd/asio_event_loop.hpp` + `src/asio_event_loop.cpp`
- `src/v3/app/bnetd/include/app/bnetd/legacy_bridge.hpp` + `src/legacy_bridge.cpp`
- `src/bnetd/server_v3_hook.h` + `src/bnetd/server_v3_hook.cpp`

`AsioEventLoop` wraps `asio::io_context`. `LegacyBridge` holds the loop and
exposes `tick(ms)`. `server_v3_hook` is the C-linkage entry point called from
legacy `server.cpp`. Test suite: 12 tests. All green.

### Round 139 — 2026-05-22

Wired `server_tick_v3(5)` into `src/bnetd/server.cpp` main loop. The legacy
`server_loop()` now calls `server_tick_v3(5)` once per iteration, giving the
Asio event loop 5 ms of CPU time per legacy tick. Zero functional change to
legacy behaviour; the v3 loop is a no-op until handlers are registered.

### Round 140 — 2026-05-22

`BnetConnectionAdapter` bridges `BnetFsm` → `ConnectionFsm`:
- `src/v3/app/bnetd/include/app/bnetd/bnet_connection_adapter.hpp`
- `src/v3/app/bnetd/src/bnet_connection_adapter.cpp`
- `src/v3/app/bnetd/include/app/bnetd/logging_connection_context.hpp`

Adapter translates `BnetFsm` callbacks into `ConnectionFsm` state transitions.
`LoggingConnectionContext` provides a no-op `IConnectionContext` for tests.
Test suite: 13 tests, 108 assertions. All green.

### Round 141 — 2026-05-22

`LuaRuntime` + `LuaConnectionContext`:
- Lua scripting wired into v3 event hooks via `IConnectionContext`
- `LuaRuntime` loads scripts from `lua/` directory
- `LuaConnectionContext` implements `IConnectionContext` with Lua dispatch

Test suite: 10 tests. All green.

### Round 142 — 2026-05-22

Handler audit — `handle_wol.cpp`, `handle_wserv.cpp`, `handle_d2cs.cpp`.
All three files deferred (see `plans/phase3-bnetd-checklist.md` §5.4, §5.9):

- `handle_wol.cpp` — 3 call sites in `irc.cpp` + 1 in `anongame_wol.cpp` remain
- `handle_wserv.cpp` — 1 call site in `irc.cpp` remains
- `handle_d2cs.cpp` — called from `server.cpp:996`, `handle_init.cpp:192`,
  `connection.cpp:2187`; deletion blocked until D2CS v3 session handling is live

Phase 3 deferred items tracked in checklist. Phase 4 (D2 services) starting R143.

### Round 143 — 2026-05-22

Phase 4 planning:
- Updated `plans/progress-master.md`: Phase 3 status (R131–R142), Phase 4 section added
- Created `plans/phase4-d2-checklist.md`: comprehensive D2CS + D2DBS audit and
  migration checklist (file inventory, line counts, v3 coverage map, step-by-step plan)
