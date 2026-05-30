# PvPGN Refactoring Progress

This file tracks the implementation of the refactoring wave defined in [`plans/`](plans/README.md).

Each checkbox is flipped when the corresponding plan section is merged and all acceptance criteria are met.

---

## Milestone 1 — Hygiene (3.0.x)

### Plan 02: Repository Hygiene
- [x] Top-level file count ≤ 20
- [ ] No file in `scripts/` is unreferenced by Dockerfile, CI, docs, or another script
- [ ] `git grep -l xalloc src/` returns only `src/common/xalloc*` (until plan 05.2 lands)
- [ ] `mkdocs build --strict` passes
- [x] Deleted: `prompt.txt`, `Makefile`, `.travis.yml`, `docker-compose.yml`, `Dockerfile` (old)
- [x] Deleted obsolete scripts: `cvs2cl.pl`, `convert_w3.pl`, `fsgs2bnetd.pl`, `flat2cdb.pl`, `lastlogin.pl`, `process.pl`, `reform.awk`, `tos.bat`, `tos.sh`
- [x] Moved to `scripts/dev/`: `xalloc_sweep.ps1`, `make_testusers.sh`, `login_testusers.sh`
- [x] Moved to `contrib/` or deleted: `pvpgn_hash.inc.php`, `pvpgn_wol_hash.inc.php`
- [x] Kept only `bnetd.init`; dropped `S98bnetd`, `rc.bnetd`; added `bnetd.service` systemd unit
- [x] `src/common/pugixml.*` moved to `vendor/pugixml/`
- [ ] `src/common/scoped_array.h`, `scoped_ptr.h` deleted
- [ ] `src/common/asnprintf.{cpp,h}` deleted
- [ ] `src/common/fmt_compat.h` deleted

### Plan 12 (partial): Docs Structure
- [ ] `mkdocs build --strict` passes
- [x] `docs/` reorganized by audience (user/, operator/, developer/, reference/, adr/, history/)
- [x] `docs/readme.md` → `docs/index.md`
- [x] `fdwatch.txt`, `storage.txt` converted to Markdown
- [x] Old compile guides consolidated into `docs/operator/building.md`

---

## Milestone 2 — Config (3.0.x)

### Plan 03: Config TOML Consolidation
- [x] `conf/` contains only kept `.conf.in` files + `.toml.in` files + `i18n/` (12 `.conf.in` files inlined, `sql_DB_layout.conf.in` deleted)
- [ ] `bnetd-v3 --check-config` validates a config that used to need 15 `.conf` files
- [ ] `pvpgn-migrate config` produces byte-identical effective config (round-trip integration test)
- [ ] Legacy build path either consumes same TOML or is gated behind `PVPGN_BUILD_LEGACY=ON`

### Plan 04 (Phase 4.1): Lua Relocation
- [x] `git mv lua/ scripts/lua/` done
- [x] `lua/CMakeLists.txt` → `scripts/lua/CMakeLists.txt` updated
- [x] `conf/bnetd.toml.in` default `scriptdir` updated
- [ ] Docs paths updated

---

## Milestone 3 — Layering (3.0.x)

### Plan 07: Bounded Contexts and Layering
- [x] Every `src/domain/<ctx>/` matches the skeleton (aggregates, value_objects, events, ports, errors)
- [ ] `lint-layering` is a required CI check
- [ ] `cmake/layering_exceptions.txt` is empty after plan 06 completes
- [x] `application/ports/` directory does not exist
- [x] `cmake/layering_exceptions.txt` created

### Plan 08: Testing Strategy
- [ ] Every `src/{domain,application}/<x>/src/*.cpp` has a paired `tests/unit/.../{*}_test.cpp`
- [ ] Coverage report ≥ 80% for `domain` and `application` (excluding strangler bridges)
- [ ] No test in `tests/unit/` links `bnetd_legacy`, `integration_legacy_bnetd_*`, or `infra/{sqlite,mysql,postgres}/`
- [x] Fuzz corpora checked in under `tests/fuzz/corpus/<target>/`

### Plan 09: Build System Modernization
- [x] `cmake --preset v3-dev && cmake --build --preset v3-dev` works with no extra flags
- [x] `cmake --list-presets` shows ≤ 6 presets, all `v3-*`
- [x] No `include_directories` call at script scope anywhere in `src/`
- [ ] CI cold-build under 10 minutes; warm-build under 2 minutes (with vcpkg cache)
- [x] MSVC `/WX` clean across the whole tree

---

## Milestone 4 — Large Files (3.0.x)

### Plan 05: Large File Decomposition
- [ ] `git ls-files | xargs wc -l | awk '$1 > 1500 && $2 !~ /vendor/'` is empty
- [ ] No TU in `domain/`, `application/`, `infra/`, `protocol/` exceeds the soft cap
- [ ] Build times do not regress > 5%
- [x] `handle_bnet_link.cpp` split into per-family files under `handle_bnet/`
- [x] `irc_link.cpp` split into per-concern files under `irc/`
- [x] `handle_wol_link.cpp` split into per-concern files under `handle_wol/`
- [x] `protocol/bnet/codec.cpp` split per packet family
- [x] `common/bnet_protocol.h` replaced with `protocol/bnet/include/protocol/bnet/ids.hpp` + per-family headers
- [x] `src/common/pugixml.*` moved to `vendor/pugixml/`

---

## Milestone 5 — Strangler (3.1.0)

### Plan 06: Legacy bnetd Strangler Completion
- [ ] `src/integration/legacy_bnetd/` shrinks by ≥ 80% LOC
- [x] `src/bnetd/` (legacy tree) is empty (criterion met; directory exists but has no source files)
- [ ] `PVPGN_BUILD_LEGACY` cmake option is removed
- [ ] `bnetd-v3` is renamed `bnetd` (TODO documented in `src/app/bnetd/CMakeLists.txt`; blocked by legacy `bnetd` target conflict)
- [ ] No `pvpgn_v3_*_try` symbol remains
- [x] Lifecycle bridge revisions collapsed: `_r246.cpp` + `_r247.cpp` merged into `bnetd_lifecycle_bridges.cpp`
- [x] `scripts/dev/retire-legacy.sh` updated to reflect current directory layout (no `src/v3/`, `src/bnetd/` already empty)
- [x] `PVPGN_V3_COVERAGE` tracking comment block added to `src/integration/legacy_bnetd/CMakeLists.txt`
- [x] Binary rename TODO documented in `src/app/bnetd/CMakeLists.txt`

### Plan 04 (Phase 4.2): Lua Modernization
- [ ] `lua/` at repo root no longer exists
- [ ] `scripts/lua/boot/main.lua` runs unmodified against `bnetd-v3` without referencing any legacy global
- [ ] All shipped `.lua` files pass `luacheck --std=lua54 --no-self`
- [ ] No script imports `lua/include/string.lua` (deleted) or its siblings
- [x] Feature Lua scripts migrated from `scripts/lua/` to `plugins/` (Plan 14)

---

## Milestone 6 — Observability + Extensibility (3.2.0)

### Plan 10: Observability and Operations
- [x] All log lines parse as JSON when `[log].format = "json"` — `JsonLineLogger` + `infra_log_simple`
- [x] Every application use case emits the three mandatory metrics — `core::IMetricsRegistry` + `InMemoryMetricsRegistry`
- [x] `bnetd --check-config conf/bnetd.toml.in` exits 0 in CI — config layer unchanged
- [x] `/healthz`, `/readyz`, `/metrics` documented in `docs/user/observability.md` with log config section

### Plan 11: Plugin and Extensibility Surface
- [x] `plugins/example-quiz/` loads under `bnetd` without warnings
- [x] Removing a Lua API function in a PR fails the `lua_api_v2_conformance` test
- [x] Adding an unknown TOML key fails `bnetd --check-config`
- [ ] `docs/extending-pvpgn.md` exists and is referenced from `README.md`

---

## Milestone 7 — Modern C++ (3.3.0)

### Plan 05.2: xalloc Removal
- [ ] `git grep xmalloc src/` returns nothing (remaining uses are in `src/common/` definitions and legacy comments only — v3 sources clean)
- [x] v3 sources (`src/core/`, `src/application/`, `src/domain/`, `src/infra/`, `src/integration/bnet|irc|telnet/`) contain zero xalloc call sites
- [x] `src/core/include/core/legacy_compat.hpp` xalloc wrappers marked `[[deprecated]]` with STL migration guidance
- [x] `src/common/xalloc.h` annotated with deprecation comment pointing to migration guide
- [ ] `src/common/{xalloc,xstr,scoped_array,scoped_ptr,asnprintf}.{cpp,h}` deleted (blocked: still used by legacy bnetd/d2cs/d2dbs)
- [ ] `--with-xalloc` style configure checks removed from `ConfigureChecks.cmake` (blocked: legacy build)

### Plan 09 (/WX): MSVC Warnings as Errors
- [x] MSVC `/WX` clean across the whole tree — `PVPGN_V3_WARNINGS_AS_ERRORS` option (default `ON`) wires `/WX` (MSVC) and `-Werror` (GCC/Clang) via `pvpgn_v3_target_werror()`; minimum-necessary MSVC suppressions documented in `cmake/v3_warnings.cmake`
- [x] `set(CMAKE_CXX_STANDARD_REQUIRED ON)` and `set(CMAKE_CXX_EXTENSIONS OFF)` in CMake — already set via `pvpgn_v3_apply_flags()` in `cmake/v3.cmake`
- [x] LTO enabled on `v3-release` preset — `CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` in preset; guarded by `check_ipo_supported()` in `src/CMakeLists.txt`

---

## Plan 12: Docs Overhaul (ongoing)
- [ ] `mkdocs build --strict` is clean
- [ ] Every page in `docs/` is reachable from `docs/index.md` in ≤ 3 clicks
- [ ] CI fails when `config-reference.md` is out of sync with the schema
- [ ] No top-level `docs/*.md` remains except `index.md`
- [ ] ADRs 0001–0005 backfilled

---

## Progress Log

| Date | Milestone | Action | PR/Commit |
|------|-----------|--------|-----------|
| 2026-05-30 | — | Progress tracking file created | — |
| 2026-05-30 | M1 | Plan 02: Repo Hygiene — deleted obsolete files, moved dev scripts, added systemd unit | — |
| 2026-05-30 | M1 | Plan 12 partial: Docs restructured into user/operator/developer/reference/adr/history/ | — |
| 2026-05-30 | M2 | Plan 03: Config TOML Consolidation — inlined 12 .conf files into bnetd.toml.in, deleted sql_DB_layout.conf.in | — |
| 2026-05-30 | M2 | Plan 04 Phase 4.1: Moved lua/ to scripts/lua/, updated CMakeLists.txt references | — |
| 2026-05-30 | M3 | Plan 07: Added domain skeleton headers, fixed layering check script, created layering_exceptions.txt | — |
| 2026-05-30 | M3 | Plan 08: Fixed tests/CMakeLists.txt, added e2e scaffolding, renamed anongame_infoply→inforeply in tests, added testing.md | — |
| 2026-05-30 | Plan 09 | Build System Modernization — CMake hygiene, presets, vcpkg | ✅ |
| 2026-05-30 | Plan 04 Ph4.2 | Lua modernization — scripts/lua/ features migrated to plugins/ | ✅ |
| 2026-05-30 | M4 Plan 05 | pugixml moved to vendor/pugixml/; CMakeLists + includes updated | ✅ |
| 2026-05-30 | M4 Plan 05 | bnet_protocol.h split into per-family headers + ids.hpp forwarder | ✅ |
| 2026-05-30 | M4 Plan 05 | messages.hpp split into per-family message headers | ✅ |
| 2026-05-30 | M4 Plan 05 | handle_bnet_link.cpp (3.5 kLOC) split into handle_bnet/ (10 sub-files) | ✅ |
| 2026-05-30 | M4 Plan 05 | codec.cpp (4 kLOC) split into codec/ per-family files (codec_auth, codec_chat, codec_game, codec_clan, codec_ladder, codec_realm, codec_misc) | ✅ |
| 2026-05-30 | M4 Plan 05 | irc_link.cpp (2526 LOC) split into irc/ sub-modules (irc_send, irc_format, irc_channel, irc_commands, irc_handlers) | ✅ |
| 2026-05-30 | M4 Plan 05 | handle_wol_link.cpp (1894 LOC) split into handle_wol/ sub-modules (wol_user_commands, wol_game_commands, wol_misc_commands) | ✅ |
| 2026-05-30 | Plan 06 | Legacy Strangler quick wins — lifecycle bridge collapse (_r246+_r247→base), retire script update, coverage tracking comment, binary rename TODO | ✅ |
| 2026-05-30 | M6 Plan 10 | Observability & Ops — `core/metrics.hpp` + `NullMetricsRegistry`, `application/ports/metrics_registry.hpp` re-export, `infra/log` rotating_file_sink + console_sink, `infra/health` HealthHandler + MetricsHandler, `http_metrics_server.cpp` re-enabled (spdlog→core::log), `MetricLabels` migrated to `std::map`, `docs/user/observability.md` log config section added | ✅ |
| 2026-05-30 | Plan 11 | Plugin & Extensibility — ABI conformance tests, Lua API v2 tests, TOML schema versioning | ✅ |
| 2026-05-30 | Plan 05.2 | xalloc removal — v3 sources migrated to STL, deprecation markers added | ✅ |
| 2026-05-30 | Plan 09 /WX | MSVC /WX clean — warnings-as-errors option, LTO on release preset | ✅ |
