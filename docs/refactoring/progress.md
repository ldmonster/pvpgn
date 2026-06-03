# Refactoring Progress

Single source of truth for executing [`plans/`](../../plans/README.md). Older
trackers (`refactoring-progress*.md`, `planstwo/*`) are historical and will be
archived under `docs/refactoring/archive/` (Milestone 0 task).

Convention: one entry per executed step. Mark each gate **build-verified**,
**reference-verified** (isolated `-Werror` compile + review, backend/runtime
absent locally), or **red** (known-failing, scheduled).

---

## Milestone 0 — Baseline & consolidation

### Step 0.1 — Local quality-gate aggregator + baseline (in progress)

**Date:** 2026-06-02 · branch `feat/overhaul`

**Done**

- Added `scripts/dev/check-all.sh` — the single local gate implementing the
  three-ring model from `plans/11-local-quality-gates.md` (ring 2 default,
  ring 3 via `--deep`). Reports PASS/FAIL/**SKIP**; skips are honest
  (missing backend/runtime), never counted as pass; exits non-zero on any hard
  failure.
- Created this consolidated tracker.

**Baseline captured (`check-all.sh --no-build`, 2026-06-02):**

| Gate | Status | Note |
|------|--------|------|
| layering rule (empty allow-list) | ✅ PASS | allow-list is genuinely empty, 0 violations — already at target |
| domain purity (`src/domain`) | ✅ PASS | no I/O/clock/global/logging in domain |
| unit pairing | ✅ PASS | every domain/app TU has a paired test |
| test↔legacy linkage | ✅ PASS | no unit test links legacy/DB-backend targets |
| plugin ABI semver | ✅ PASS | matches v1 golden |
| plugin ABI purity (C99) | ✅ PASS | strict-C99 clean, C stdlib includes only |
| changelog discipline | ✅ PASS | Keep-a-Changelog |
| config reference sync | ❌ **RED** | docs mention `Environment`/`Config`/`server` sections absent from `bnetd.toml.in` |
| docs reachable | ❌ **RED** | `adr/0009-modules-pilot.md`, `adr/0011-runtime-image.md` unreachable from `docs/index.md` |
| no orphan scripts | ❌ **RED** | `scripts/storage/{cdb2sql,plain2sql}.pl` + several `scripts/lua/*` unreferenced |
| build + unit/functional | ∅ SKIP | needs a configured `build/v3-dev` |

**Summary:** 7 passed, 3 failed, 1 skipped.

**Correction to `plans/02-current-state.md`:** that doc lists the layering /
purity allow-lists as "non-empty (debt)". They are in fact **empty and green**
today — the M2/M3 allow-list-emptying target is already met for layering and
purity. The real remaining structural debt is narrower (legacy bridges, protocol
test depth, e2e breadth, the 3 red gates above).

### Step 0.2 — Fix red ring-2 gates (done)

**Date:** 2026-06-02 · user decisions: orphans = "delete dead + reference live";
config-sync = "mark env-gated".

- ✅ **docs reachable** — `docs/index.md` listed ADRs 0001–0005 only; added
  0006–0011 + the ADR template. `check-docs-reachable.sh` exits 0.
- ✅ **no orphan scripts** — deleted 15 genuinely-dead one-shot migration
  scripts (`migrate_bridges{,_phase1,_phase2}.py`, `plan02_common_purge.py`,
  `plan05_{consolidation,finalize}.py`, `split_{codec,handle_bnet,handle_wol,
  irc_link}.py`, `strip_v3_guards.py`, `update_cmake_after_deletion.py`,
  `printf_to_print.py`, `probe-toml-subst.sh`, `xalloc_sweep.ps1`). Cataloged
  the 39 live assets (shipped `scripts/lua/*` API, doc generators, deployment
  units, localize/sql/storage utilities) in a new, genuinely-useful
  `docs/developer/scripts-and-tooling.md`, wired into mkdocs nav + linked from
  `index.md`. `check-scripts-orphans.sh` exits 0 (0 orphans).
- ✅ **config reference sync** — handled as **env-gated** per decision.
  `check-all.sh` now skips it honestly when `pvpgn_config_tool` is not built
  (the binary that regenerates the doc via `--print-schema`). Underlying debt
  recorded for Milestone 6 (docs): reconcile the ~30 `bnetd.toml.in` sections
  not covered by the typed config model / generated doc, then regenerate.

**Gate state now (`check-all.sh --no-build`): 9 passed, 0 failed, 2 skipped**
(config-reference-sync env-gated; build skipped). check-all exits 0.

### Step 0.3 — Ring-1 pre-commit hooks (done)

**Date:** 2026-06-02

- Added a `repo: local` block to `.pre-commit-config.yaml` with three fast,
  build-free architectural hooks: `v3-layering` (dependency rule),
  `domain-purity` (`src/domain`), and `changelog-discipline`. Each is scoped via
  `files:` so it only runs when relevant files change.
- Made `scripts/v3_layering_check.sh` executable (was `-rw-`, required by
  `language: script`).
- **Verified:** YAML valid; all three hook entries exit 0 when run directly.
- **Env-gated:** `pre-commit` is not installed on this box, so an end-to-end
  `pre-commit run --all-files` is reference-verified only.

### Step 0.4 — CTest band labels (done, build-verified)

**Date:** 2026-06-02

- Added an optional `LABEL` arg to `pvpgn_v3_add_test` (`cmake/v3.cmake`); when
  absent it auto-derives `functional` for tests under `tests/functional/` and
  `unit` otherwise, then passes `PROPERTIES LABELS` to `catch_discover_tests`.
  The helper is used only by `tests/unit` (75 dirs) + `tests/functional` (1);
  `tests/integration` and `tests/e2e` already label themselves, so no conflict.
- **Build-verified** (not just reference): configured `build/v3-dev` with
  `-DPVPGN_V3_WITH_LUA=OFF` (Lua 5.4 absent locally — env constraint), built
  `test_application_icon_table` + `test_functional_config_roundtrip`, and
  confirmed `ctest -L unit` → 10/10 pass (icon_table), `ctest -L functional` →
  5/5 pass (config_roundtrip). Labels select the correct band.
- Note: the canonical `v3-dev` preset has Lua ON; on a Lua-equipped box no
  `-D` override is needed. `check-all.sh`'s build band now runs against
  `build/v3-dev` instead of skipping (currently only the 2 demo targets are
  built; building the full suite is a separate step).

### Step 0.5 — Lua ON + full build/test with all backends (done, build-verified)

**Date:** 2026-06-02 · user directive: "turn lua on, fix all errors and
warnings, build and test completely, also build dockerfile and windows
dockerfile."

**Local deps without root.** sudo is locked and Lua/sqlite/sodium were absent.
`apt-get download` + `dpkg-deb -x` into `.localdeps/prefix` (gitignored) gives
Lua 5.4.6, sqlite3, libsodium dev headers+libs; `.localdeps/env.sh` points
CMake/pkg-config/loader at them. Configured `v3-dev` with
`PVPGN_V3_WITH_LUA=ON` + sqlite + sodium all found.

**Source/build fixes (all real, not local-only hacks):**

1. `src/infra/scripting/CMakeLists.txt` — probed `-Wno-template-body`, which GCC
   silently accepts as an unknown `-Wno-*`, so the check passed on GCC 13 and
   then `-Wno-error=template-body` hard-errored. Now probes the **positive**
   `-Wtemplate-body` (correctly absent on GCC < 14).
2. `src/infra/sqlite/CMakeLists.txt` — backend linked a bare `sqlite3` and never
   added the discovered header dir, so it only built when sqlite was on the
   default paths. Now consumes `PVPGN_V3_SQLITE3_INCLUDE_DIR/_LIBRARY`, and
   links `infra_persistence` (its `unit_of_work.cpp` instantiates the
   consolidated `Sql*Repository` classes whose vtables live there — previously
   an undefined-vtable link error in `pvpgn-migrate`).
3. `src/CMakeLists.txt` — libsodium linked via bare `${SODIUM_LIBRARIES}`
   (no `-L`); switched to `pkg_check_modules(... IMPORTED_TARGET ...)` +
   `PkgConfig::SODIUM` so it links from any prefix.
4. **`src/scripting/plugin/src/plugin_manifest.cpp` — real heap-use-after-free**
   (found via targeted ASan/UBSan build): `trim()` returned a `std::string`
   temporary and `unquote()` did `string_view = trim(view)`, leaving the view
   dangling. Made `trim` return `std::string_view` (narrows the live buffer,
   no alloc); fixed the 3 `std::string x = trim(...)` call sites. This had
   corrupted parsed plugin IDs (`PluginABIConformance` failure).
5. `src/CMakeLists.txt` — toml++'s internal `assert()` ABORTED on malformed
   input in Debug (defeating the "config parsing never crashes" property test).
   Defined `TOML_ASSERT(expr)=` via `target_compile_options` on `infra_config`
   (CMake filters function-like macros out of `target_compile_definitions`), so
   Debug matches Release/NDEBUG and bad input returns a parse error.

**Result:** full `v3-dev` build **0 errors / 0 warnings**; `ctest`
**2581/2581 pass** (was 3 failing: the 2 toml property tests + plugin ABI
conformance — all now green). These tests only run with the plugin/Lua/sqlite
runtimes present, so they were previously env-gated and unverified.

**Remaining for this directive:** build `Dockerfile.v3` (canonical Lua-on
build+test in a clean image), `Dockerfile` (legacy), and `Dockerfile.windows`.

### Step 0.6 — Docker images (in progress)

**Date:** 2026-06-02

- **`.dockerignore`** — added `.localdeps/` so the local dep prefix never enters
  a build context.
- **`Dockerfile.v3` (canonical v3, Lua ON) — ✅ built + tested in-container.**
  `--target v3-test` runs the clean-room Alpine (GCC 15.2, real
  `lua5.4-dev`/`sqlite-dev`) build and the full suite: **2581/2581 pass, 0
  failed.** Image `pvpgn-v3-test:local` (2.14 GB). Independent confirmation of
  the native result.
- **`Dockerfile` (legacy/multi-backend) — fixes applied, 2 issues found:**
  1. `find_package(Lua 5.4 REQUIRED)` failed (build-base had no Lua). Added
     `lua5.4-dev` + `ENV CMAKE_INCLUDE_PATH/LIBRARY_PATH` so the mandatory v3
     sub-tree resolves Lua. ✅
  2. `make` had no `-j` (single-threaded, ~5%/90s). Now `make -j"$(nproc)"`. ✅
  3. **Real v3 MySQL-backend bugs** exposed once `mariadb-dev` was present
     (locally the backend was a stub): `connection.cpp` used `std::strlen`
     without `<cstring>`; `account_repository.cpp` bound a `string_view` to
     `sql_escape(const std::string&)`. Fixed: added `<cstring>`; made
     `sql_escape` take `std::string_view` (dropped 2 redundant `std::string{}`
     wraps); silenced 2 `[[nodiscard]]` warnings on fire-and-forget queries.
     Verified locally by downloading MariaDB dev libs and building
     `pvpgn_infra_mysql` → **0 errors / 0 warnings**.
  - **Compiles 100% clean in-container now**, but the image-assembly step fails:
    `cp: can't stat '/etc/pvpgn'`. Root cause: `conf/CMakeLists.txt` gates
    config install on `WITH_BNETD`/`WITH_D2CS`/`SYSCONFDIR` — **stale variables
    that no longer exist** post-v3-migration (the v3 `bnetd` target now builds
    unconditionally; the Dockerfile passes `SYSCONF_INSTALL_DIR`, not
    `SYSCONFDIR`). So no TOML/json configs install and `/etc/pvpgn` is never
    created. Legacy-image packaging drift — surfaced to user for a decision.
- **`Dockerfile` (legacy) — now BUILDS clean AND runs.** After the conf/files
  install fix below, image `pvpgn-legacy:local` (40 MB) builds with 0
  errors/warnings; `bnetd` loads (added `lua5.4-libs` to the runner — needed
  now that Lua is ON), starts, and **listens on 6112/4000/6667** logging to
  stdout. Fixes:
  - `conf/CMakeLists.txt` + `files/CMakeLists.txt` + `conf/i18n/CMakeLists.txt`:
    install gating used dead vars (`WITH_BNETD`/`SYSCONFDIR`/`LOCALSTATEDIR`);
    rewired to canonical `SYSCONF_INSTALL_DIR`/`LOCALSTATE_INSTALL_DIR`, install
    unconditionally (v3 servers always build).
  - top `CMakeLists.txt`: added `add_subdirectory(conf)` + `add_subdirectory(files)`
    — they were never added, so `make install` shipped no configs/data at all.
  - `Dockerfile`: `lua5.4-dev` + CMake search-path env in build-base;
    `make -j`; `lua5.4-libs` in runner; CMD → `bin/bnetd -c bnetd.toml` (was
    `sbin/bnetd -f .conf`, all wrong for v3).
  - `scripts/docker-entrypoint.sh`: modernized for v3 — use `bnetd.toml`, sed
    `${SYSCONFDIR}`/`${LOCALSTATEDIR}` placeholders (left literal by
    `configure_file(@ONLY)`) to runtime dirs, drop the legacy `.conf`
    logfile-patch/tail dance (v3 logs to stdout).
- **✅ Runtime daemon bug FIXED + verified.** `bnetd`/`d2cs` bound their ports
  then exited immediately: `IoRuntime::run()` only spawns workers (returns
  immediately, per its contract) and both mains treated it as blocking; the
  signal handler also called `stop()` from a worker thread (self-join). Fix in
  `src/infra/net/{src,include}/.../io_runtime.{cpp,hpp}`: split into
  `request_stop()` (signal-safe, no join — used by the signal handler) and
  `wait()` (owner-thread join); `stop()` = request_stop()+wait(). Both mains
  now do `rt.run(n); rt.wait();`. **Verified:** the legacy container stays Up
  (listening on 6112/4000/6667) and shuts down cleanly on SIGTERM in ~0.13 s
  (exit 0). Full suite still 2581/2581.
- Also switched the v3 MySQL backend to `pkg_check_modules(... IMPORTED_TARGET)`
  + `PkgConfig::MYSQLCLIENT` (same bare-`-lmariadb`-without-`-L` issue as
  sodium), so it links from any prefix.

### Step 0.7 — Windows cross-build (in progress)

- `Dockerfile.windows` (MinGW-w64 + vcpkg, `PVPGN_V3_WITH_LUA=OFF`):
  1. Added `autoconf-archive` to the toolchain (libsodium's vcpkg port requires
     it for autoreconf). ✅ got past that.
  2. libsodium then failed again — `libtool could not find a file being linked
     against` — a known broken `libsodium:x64-mingw-static` vcpkg port
     (third-party, not our code). Since **only** the Windows build uses vcpkg,
     marked `libsodium` `"platform": "!windows"` in `vcpkg.json` (mirrors how
     `zlib` is windows-only); the Windows v3 binaries build without it (crypto
     falls back to `std::random_device`, argon2id-at-rest not compiled — the
     same env-gated behavior as elsewhere).
  3. Transient `curl error 56` downloading `boost-pool` (vcpkg mis-classified
     as non-retryable) — fixed by retry.
  4. Added buildkit cache mounts (`/root/.cache/vcpkg`, `/opt/vcpkg-downloads`)
     to the build RUN so retries restore prebuilt mingw deps instead of
     recompiling Boost from scratch — makes iteration feasible.
  5. **Current blocker (deep, third-party):** the v3 CMake configure fails with
     *"Could not find a package configuration file provided by boost_headers"*.
     The mingw branch in `src/CMakeLists.txt` manually `include()`s vcpkg's
     `BoostConfig.cmake`; its nested `find_package(boost_headers CONFIG)`
     resolves against the **host** triplet (`x64-linux`), not the target
     (`x64-mingw-static`). Prepending the triplet root to `CMAKE_PREFIX_PATH`
     did not fix it (the vcpkg find_package wrapper redirects). A clean fix
     means restructuring the Boost integration (e.g. force
     `find_package(Boost CONFIG)` for both platforms instead of the manual
     include) — risky to the green native build, so it needs careful, dedicated
     work + re-verification of the Linux build. **Not a code bug in pvpgn; a
     vcpkg cross-triplet integration issue.**
  6. **Boost integration restructured** (`src/CMakeLists.txt`): replaced the
     fragile manual `include(BoostConfig.cmake)` mingw hack with a unified
     `find_package(Boost 1.75 REQUIRED CONFIG COMPONENTS ...)` for both
     platforms (CONFIG mode loads all installed component configs, so
     Boost::asio/headers appear without listing). **Native re-verified green:
     2581/2581 tests pass** with the new Boost find. For the cross build it sets
     `Boost_DIR` + prepends the triplet prefix (vcpkg-guarded; native
     untouched). This fixed top-level Boost discovery on mingw (`Boost` now
     found) and the original `boost_headers` call-site.
  - **Remaining Windows blocker (deep vcpkg internals):** BoostConfig's nested
     `find_package(boost_headers)` (and the other modular components) still are
     not resolved by the vcpkg find-package wrapper for the `x64-mingw-static`
     triplet, even with the prefix set. Cracking this needs per-component
     `boost_*_DIR` overrides or a vcpkg overlay/port change — a vcpkg-specific
     intervention, not a pvpgn code issue. Left here after 8 build iterations.
  - **Net Windows progress:** toolchain (autoconf-archive), libsodium
     (`!windows`), boost-pool net flake (retry), cache mounts, and Boost
     top-level discovery all resolved; the modular-boost-component resolution
     under vcpkg cross-triplet is the sole remaining blocker.

  7. **CMake upgraded to 3.31.6** in the Windows toolchain (bookworm ships 3.25,
     too old for Boost 1.91 — its FindBoost treats header-only `system` as a
     missing lib). Confirmed in use; did **not** resolve the nested
     `boost_headers` config resolution.
  - **Definitive conclusion after 10 build iterations:** vcpkg's *modular* Boost
     component configs (`boost_headers`, etc., reached via `BoostConfig.cmake`'s
     nested `find_package`) are **not resolvable for the `x64-mingw-static`
     cross-triplet** in this setup — independent of CMake version (3.25→3.31),
     `Boost_DIR`, `CMAKE_PREFIX_PATH`, or find mode (CONFIG fails on nested
     `boost_headers`; MODULE fails because FindBoost + the vcpkg-cmake-wrapper
     can't reconcile modern modular Boost). Resolving it needs a different
     strategy — a vcpkg overlay/port fix, a pinned older Boost, or non-vcpkg
     Boost for Windows — i.e. dedicated vcpkg-specialist work, **not a pvpgn
     code bug**.
  8. Pinned `VCPKG_REF` to stable `2026.05.25` (was `master`) and re-tested:
     **boost_headers fails identically on the stable release** — so it is NOT a
     transient-master issue. Confirmed via a `share/` layout diagnostic that the
     modular `boost_headers` CMake config the meta `BoostConfig.cmake` requires
     is not resolvable for `x64-mingw-static` in this vcpkg layout.
  - **Final root cause (definitive, after 12 builds):** vcpkg's modular Boost
     (1.91) `BoostConfig.cmake` does `find_package(boost_headers)`, which the
     vcpkg cross-triplet (`x64-mingw-static`, host `x64-linux`) find wrapper
     cannot resolve — **independent of CMake version (3.25/3.31), find mode
     (CONFIG/MODULE), `Boost_DIR`/prefix, and vcpkg master-vs-release.** Needs a
     vcpkg-specialist fix: an overlay port for boost, an older pinned Boost
     whose config is cross-friendly, or dropping vcpkg Boost on Windows for a
     system/prebuilt mingw Boost. Tracked as the sole Windows follow-up.
  - **Resolved along the way (all real, kept):** toolchain `autoconf-archive`;
     `libsodium` `!windows`; boost-pool net-flake retry; vcpkg binary-cache +
     downloads buildkit cache mounts; the Boost integration restructured to a
     unified `find_package(Boost CONFIG)` (native re-verified **2581/2581**);
     top-level Boost discovery via `Boost_DIR`; CMake 3.31.6; `VCPKG_REF`
     pinned to a stable release (reproducibility).

**Overall:** all directive items complete and verified EXCEPT the Windows
cross-compile image, blocked on a third-party vcpkg/mingw modular-Boost
cross-triplet issue (fully isolated + documented; needs a dedicated vcpkg
strategy). The v3 server builds 0/0 and passes **2581/2581** tests natively and
in `Dockerfile.v3`; the legacy `Dockerfile` produces a working, runnable image
(`bnetd` stays up + clean SIGTERM).

**Other M0 remainder (deferred):** archive legacy trackers; broaden
`check-all.sh` build band.

---

## Milestones 1–6

Not started. See [`plans/14-migration-roadmap.md`](../../plans/14-migration-roadmap.md).
