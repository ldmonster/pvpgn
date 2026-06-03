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
  - **Final root cause (definitive, after 15 builds):** vcpkg's modular Boost
     `BoostConfig.cmake` does `find_package(boost_headers)`, which the vcpkg
     cross-triplet (`x64-mingw-static`, host `x64-linux`) find wrapper cannot
     resolve — **fully version-INDEPENDENT.** Confirmed across: CMake 3.25 and
     3.31; CONFIG and MODULE/plain find modes; with/without `Boost_DIR`+prefix;
     vcpkg master (Boost 1.91) AND tagged releases (Boost 1.88 @ 2025.06.13,
     also 1.86/1.87 available). MODULE mode fails differently but as fatally
     (CMake FindBoost can't model header-only Boost.System → "missing system").
     So it is NOT a Boost-version, CMake-version, or find-mode problem — it is
     the vcpkg cross-triplet find-wrapper / prefix resolution itself. Needs a
     vcpkg-specialist fix: a custom `vcpkg-cmake-wrapper` for boost, an overlay
     port, or building Boost for mingw outside vcpkg. Sole Windows follow-up.
  - *(The "blocked / dedicated vcpkg-specialist work" framing above was the
     status mid-investigation; it was subsequently RESOLVED — see below. The
     narrative is kept as a record of how the root cause was isolated.)*

### Step 0.8 — Windows cross-build RESOLVED ✅

The vcpkg cross-triplet find-wrapper is broken for **every** compiled dep
(Boost, zlib, OpenSSL), so the winning pattern was to **bypass the wrapper and
wire the vcpkg-installed artifacts directly**, plus a newer toolchain:

- **Toolchain:** `debian:trixie-slim` (GCC 14 mingw — has `<format>`; bookworm
  GCC 12 didn't); CMake 3.31; `autoconf-archive`; vcpkg cache mounts;
  `VCPKG_REF=2025.06.13` (Boost 1.88); `libsodium` `!windows`;
  `boost-beast`/`boost-multiprecision`/`openssl` added; `VCPKG_APPLOCAL_DEPS=OFF`.
- **Boost:** header-only INTERFACE shim (Asio/system/headers are header-only;
  fiber off on Windows) pointing at the vcpkg include dir + `ws2_32`/`mswsock`.
- **zlib:** skip the broken vcpkg find; FetchContent builds from source; exclude
  its shared-DLL target (fails on mingw-static).
- **OpenSSL:** `OpenSSL::SSL/Crypto` imported targets from the vcpkg static libs
  by **direct path** (cross `find_library` only searches the sysroot), with
  Windows link deps (`crypt32`/`ws2_32`/…).
- **Source ports** (all `#ifdef _WIN32`/cross-guarded — native unchanged):
  `win_service.cpp` (`core::Error`), `crash_handler.cpp` (`strsignal`),
  `signal_handler.cpp` (POSIX-only signals), `server_config.cpp` (`_environ`),
  `infra/file/account_repository.cpp` (`O_CLOEXEC`/`_commit`/narrow path),
  `http_metrics_server.cpp` (`make_address` — `from_string` removed in Boost
  1.87), `messages_misc.hpp` (`#undef MessageBox` — windows.h's
  `#define MessageBox MessageBoxA` mangled the protocol type's symbol).
- **Artifacts:** `dist/windows/bin/{bnetd,pvpgn_v3_d2cs,pvpgn_config_tool,
  pvpgn-conf-convert}.exe` — all `PE32+ x86-64 MS Windows` (bnetd 17 MB).

**Overall — ALL directive items complete and verified:** the v3 server builds
0 errors / 0 warnings and passes **2581/2581** tests natively (Lua+sqlite+sodium
ON) and in `Dockerfile.v3`; the legacy `Dockerfile` produces a working, runnable
image (`bnetd` stays up + clean SIGTERM); and `Dockerfile.windows`
cross-compiles 4 working Windows executables. (Native `-j24` runs can flake on
the raw-fd `adopt_native_handle` tests under parallel pressure — a pre-existing
isolation quirk; `-j8` is 2581/2581 green.)

### Step 0.9 — Consolidate trackers (done)

**Date:** 2026-06-03

- Archived the 6 root `refactoring-progress*.md` trackers and the entire
  `planstwo/` tree into `docs/refactoring/archive/` (git mv, history preserved).
  `docs/refactoring/progress.md` is now the single live tracker.
- `mkdocs.yml`: added `exclude_docs: refactoring/` so the internal tracker +
  archive don't pollute the published site or trip `mkdocs build --strict`
  (mkdocs is env-gated — not installed locally — so reference-verified).
- Verified `check-docs-reachable.sh` and `check-scripts-orphans.sh` still pass
  after the move.

**M0 status:** essentially complete (aggregator, pre-commit hooks, CTest labels,
tracker consolidation all done). One gate now RED, see below.

### Step 0.10 — Fix `config-reference-sync` (done) → check-all green

**Date:** 2026-06-03

Once the native build produced `pvpgn_config_tool`, `check-all.sh` stopped
skipping `config-reference-sync` and it failed. Root cause was twofold:
1. `docs/developer/config-reference.md` was **stale** — generated by an older
   tool (used `### [section]` headings; current tool emits `## [section]` + its
   own title). Regenerated via `gen-config-docs.sh` (deterministic; re-run is
   byte-identical).
2. The **check compared against the wrong source**: it diffed the doc's section
   list against `conf/bnetd.toml.in`. But per `plans/13` the reference doc is
   generated from the *config schema* (the typed model, via `--print-schema`),
   while `bnetd.toml.in` is a richer operator template (~47 sections incl.
   example/optional/D2/icon sections the typed bnetd schema doesn't expose).
   Rewrote `check-config-reference-sync.sh` to the correct, plan-aligned
   invariant: **regenerate from the tool and diff against the committed doc**
   (stale ⇒ fail; tool absent ⇒ exit 2, which `check-all` treats as an
   env-gated skip). Not a weakening — it's the documented "generated, kept in
   sync" gate.

**Result:** `check-all.sh --no-build` = **10 passed, 0 failed, 1 skipped**
(build band only). `check-all` is green again.

### Step 0.11 — Extend `--print-schema` to full ServerConfig (done)

**Date:** 2026-06-03

Audited the `bnetd.toml.in` sections `--print-schema` didn't expose. Finding:
`--print-schema` (a hand-coded heredoc in `src/app/pvpgn-config/main.cpp`) was
**stale vs the `ServerConfig` struct** — 8 sub-structs that exist and ARE parsed
were undocumented: `d2cs`, `localization`, `downloads`, `client_verification`,
`net.timeouts`, `status`, `command_log`, `observability`. Added all 8 (accurate
TOML keys + code defaults from `server_config.hpp`). The tool now emits **25
sections** = `[server]` + all 24 `ServerConfig` members; regenerated
`config-reference.md`. Tool rebuild 0/0; `ctest -R config|toml` 58/58;
`check-all` green.

**Boundary (correctly out of bnetd's `--print-schema`):** the remaining
`bnetd.toml.in` sections — `anongame.infos.*`, `icons.*`, `chat.topics`,
`chat.aliases`, `admin.command_groups`, `support`, `tournament` — are **not**
`ServerConfig`; they're consumed by separate subsystems (icon-table / anongame /
channel / admin-command loaders, and `d2cs.toml`). They belong to those
components' own schemas, not the bnetd server config reference.

**M0 remainder (optional):** broaden `check-all.sh` build band (build full suite).

---

## Milestone 1 — raise the test floor (in progress)

### Step 1.1 — e2e fake-client harness: groundwork + findings

**Date:** 2026-06-03 · approach chosen: build the real client tools + drive a
spawned bnetd.

Done / verified:
- **Fixed a real CMake bug** (`src/CMakeLists.txt`): the `<print>` feature probe
  compiled with `-std=c++23 -std=c++20` (the global `CMAKE_CXX_STANDARD=20` was
  appended after the required flag, so the *last* `-std` won), making
  `PVPGN_V3_HAVE_STD_PRINT` **always false even on GCC 14** — i.e. the
  print-using client tools (`bnchat`/`bnbot`/`bnstat`/`bnpass`/…) were NEVER
  built, on any toolchain, contradicting the "CI compiler-matrix builds them"
  note. Now drives the standard via `CMAKE_CXX_STANDARD` for the probe. On GCC
  13 it still (correctly) fails (no `<print>`); on GCC 14+ it now succeeds.
- **Installed GCC 14 locally without sudo** (`apt-get download g++-14 +
  *-x86-64-linux-gnu` → `.localdeps/prefix`; system libstdc++ already has
  GLIBCXX_3.4.33 so gcc14 `std::print` binaries run). Configured
  `build/v3-gcc14`; `PVPGN_V3_HAVE_STD_PRINT` now **Success** → built
  `bnchat`/`bnbot`/`bnstat` (0/0).
- **Harness mechanics proven**: a spawned gcc13 `bnetd` with
  `[persistence] backend="inmemory"` on an ephemeral port boots, logs
  `BNet/BNFTP listening on 0.0.0.0:<port>` (readiness signal), and tears down
  cleanly. `bnchat` connects and completes the init-class handshake.

Earlier finding (re the stock client tools): the v3 bnet FSM
(`src/protocol/bnet/src/fsm/fsm_auth.cpp`) implements the **modern** auth flow
(`AuthInfo → AuthCheckReply → LogonResponse2 → LogonResponse2Reply`) but the
**legacy** `on(LoginReq1)`/`on(CreateAccount1Request)` are no-op stubs. The
stock tools (`bnchat -c CHAT`) speak the *legacy* flow, so they cannot drive a
login against this bnetd. Decision: write a **modern-flow Python client** that
speaks the flow bnetd actually implements, and drive a *real* spawned bnetd.

### Step 1.1 — RESULT: green modern-flow login journey + 3 server crash fixes

**Date:** 2026-06-03 · DONE.

New test `tests/e2e/modern_login_journey_test.py` (stdlib only, no docker):
spawns a real gcc13 `bnetd` (inmemory backend, ephemeral bnet port), then a
Python client drives the modern SID handshake over the wire and asserts:
- accept: `AUTH_INFO → AUTH_CHECK{0} → LOGONRESPONSE2(e2euser) → reply 0x00`;
- reject: empty username → `LOGONRESPONSE2 reply 0x01`.
Registered as ctest `e2e.modern_login_journey` (label `e2e`, `RUN_SERIAL`,
gated by `-DPVPGN_V3_E2E_TESTS=ON`). 5/5 stable; full unit suite still 2581/2581.

This is the **first** test to drive real bnetd's wire dispatch / session-context
send / connection-teardown paths end to end (the existing `*-smoke.sh` tests
drive real client tools against a Python *mock* server, so these paths were
never exercised). It immediately surfaced **three latent SIGSEGV/contract bugs**,
all now fixed:

1. **Dispatch use-after-free** (`main/bnet_bnftp_dispatch.cpp`): the connection
   handler *is* `TcpSession::on_bytes_`; it called `tcp->set_on_bytes(...)` to
   rewire future reads, which move-assigns the very `std::function` being
   executed → destroys the running lambda and frees its captures (`tcp`,
   `peek_buf`). Subsequent use of those captures crashed. Fixed by snapshotting
   `tcp`/`peek_buf`/`this` into stack locals (`session`/`pbuf`/`self`) before the
   rewire. **Every bnet connection crashed bnetd on the first packet.**

2. **Replies never sent** (`protocol/bnet/session_context_impl.hpp`):
   `BnetSessionContextImpl::send()` called `finalize_bnet_packet()` a second
   time, but each `encode()` already finalizes its own packet → the redundant
   call returned `FailedPrecondition` and `send()` bailed *before*
   `egress_->send()`. **No `ServerMessage` ever reached the wire.** Fixed by
   dropping the redundant finalize.

3. **`on_close` dangling-`this`** (same file as #1): the close handler was built
   *after* the `set_on_bytes` rewire, so its `[this, …]` capture re-read `this`
   from the just-freed peek-lambda closure → crash on disconnect in
   `session_mgr_.unregister_session`. Fixed by the `self` snapshot from #1.

All three were invisible to the existing tests; the e2e harness is exactly the
"raise the floor" instrument M1 calls for. Login is still a permissive stub
(`login_user` use-case is left null in `app/bnetd/main.cpp`, so any non-empty
username is accepted with `0x00`); the test pins that observable contract so the
future wiring of real credential checks shows up as a deliberate change.

### Step 1.2 — codec coverage audit + session-context regression unit test

**Date:** 2026-06-03 · DONE.

Audited the protocol test floor before adding more: the bnet **codec** is
already deeply covered — `codec_test.cpp` alone has **213** cases (round-trips,
golden wire-byte parity vs legacy, oversize/short/malformed rejection across
auth/chat/clan/game/friends/userdata), plus `codec_property_test.cpp`
(round-trip identity + arbitrary-packet decode-fuzz) and `golden_replay_test.cpp`.
So "golden vectors + round-trip + fuzz" for the codec is effectively complete;
adding more there would be low-value duplication.

The real hole was one layer up: the production `BnetSessionContextImpl`
(encode → finalize → egress) had **zero** tests — every FSM test uses
`CapturingSessionContext`, which mocks `ISessionContext` and records
`ServerMessage`s *before* encoding, so the actual send path (where the
double-finalize bug lived, Step 1.1 #2) was never exercised. Added
`tests/unit/protocol/bnet/session_context_impl_test.cpp` (6 cases): one send →
exactly one well-framed packet on a capturing `IConnectionEgress`, byte-for-byte
round-trippable via `decode_server`; multi-send ordering; `close()` forwarding;
null-egress error path. Reintroducing the double-finalize makes the egress
receive nothing and fails the first assertion. Suite now 2587/2587.

### Step 1.3 — extend the e2e journey past login (chat path)

**Date:** 2026-06-03 · DONE.

Grew `modern_login_journey_test.py`'s accept path beyond login, over the same
real connection: `PING` (asserts the server mirrors the cookie verbatim),
`ENTER_CHAT` (LoggedIn→InChat; asserts `unique_name` echo), and `JOIN_CHANNEL`
(InChat; asserts a structured `SID_CHATEVENT` reply). This drives the chat FSM,
the `join_channel` use-case, and the `ChatEvent` encoder end to end over the
wire — paths the unit/FSM tests only reach with a mocked `ISessionContext`.

Observed: `JOIN_CHANNEL` currently returns `EID_INFO "Failed to join channel"`.
The join *use-case* succeeds (channel auto-created, member admitted) but its
account lookup fails — the permissive login stub leaves `current_account_id_`
at 0 and no account 0 exists (`AccountNotFound`). Same root cause as the
permissive-login finding (login_user unwired). The test pins this and accepts
the future `EID_CHANNEL` success once login_user is wired, so the flip is a
deliberate, reviewed change. Still 5/5 stable; suite 2587/2587.

### Step 1.4 — wire the e2e journey into the local dev gate

**Date:** 2026-06-03 · DONE.

Added an `e2e modern login journey` gate to `scripts/dev/check-all.sh` (Ring 2,
after the unit/functional bands). It runs the self-contained Python journey
directly against `build/v3-dev/.../bnetd` — not via `PVPGN_V3_E2E_TESTS=ON`,
which would also register the `*-smoke.sh` tests that need client binaries this
toolchain can't build. Env-gated per the script's honesty rule: SKIP (not FAIL)
when `bnetd`/`python3` is absent or under `--no-build`. With bnetd built it is a
HARD gate, so the three Step 1.1 crashes can no longer regress unnoticed
locally. Verified: `check-all` reports **13 passed, 0 failed, 0 skipped**.

### Step 1.5 — make login real: wire login_user + OLS account creation

**Date:** 2026-06-03 · DONE.

Closed the root gap behind the permissive-login / `account_id 0` findings.

- **Wired the auth use-cases** in `app/bnetd/main.cpp`: `login_user` and a new
  `create_account` over the shared in-memory account repo (+ ip-ban repo, event
  bus, `SystemClock`), plus `account_repo`/`session_registry` into the
  `BnetUseCaseContext` (added a `create_account` field).
- **Implemented `on(CreateAccount1Request)`** (SID_CREATEACCTREQ1, the OLS
  create flow) — was a no-op stub. Packs the 5×u32 hash1 to a 20-byte BNHash
  and calls the `create_account` use-case; replies OK/No.
- **Fixed a 20-byte-packing bug** in `on(LogonResponse2)`: it built the password
  BNHash from a *decimal colon string* (`"286331153:…"`), which is never the
  20 bytes `BNHash::from_bytes` requires, so a wired `login_user` would have
  rejected every login with 0x02. Now packs the 5 words as 20 LE bytes via a
  shared `pack_hash1_le` helper used by both create and login (so a created
  password round-trips to a successful login).
- **Fixed a double-attach bug**: `LoginUser::execute` already attaches the
  session (single-session policy), but the FSM passed `session = 0` to the
  use-case and then attached `session_id_` again → "account already has a
  session" → `reject()` closed a *valid* login. Now passes the real
  `session_id_` into the request and drops the redundant FSM attach.

Result: the e2e journey's accept path is now a full real login — CREATEACCT1 →
LOGONRESPONSE2 **0x00** → PING → ENTER_CHAT → **JOIN_CHANNEL EID_CHANNEL
success** (real `account_id` flows into the chat use-case) — plus genuine
credential rejections: wrong password → **0x02**, unknown account → **0x01**.
6/6 stable; full suite 2587/2587; `check-all` green.

### Step 1.6 — FSM-level unit tests for the real auth path (+ a build fix)

**Date:** 2026-06-03 · DONE.

Added `tests/unit/protocol/bnet/fsm_auth_create_login_test.cpp` (6 cases): drives
`BnetFsm` with the genuine `LoginUser`/`CreateAccount` use-cases over in-memory
repos (no sockets) to lock the Step 1.5 behaviour as fast unit tests —
CREATEACCTREQ1 creates an account; create→login succeeds (proving the 20-byte
hash1 packing round-trips); wrong password → 0x02; unknown account → 0x01;
create without the use-case refuses (no false ACK); duplicate create refused.
The success case also asserts the context is *not* closed and the session is
attached exactly once (guards the no-double-attach fix).

**Incidental build fix:** reconfiguring flipped on the system GTest (config
mode), which surfaced that the legacy GTest plugin tests (`semver_test.cpp`,
`dependency_resolver_test.cpp`) no longer compiled — they use `result->member`,
but `core::Result` had no `operator->`. That is a real build break on *any*
GTest-enabled environment, independent of this work. Added `operator->` /
`operator*` to `core::Result` (additive, mirrors `std::expected`); both legacy
suites now build and pass. Full suite **2595/2595**.

### Step 1.7 — durable accounts: file backend + restart e2e

**Date:** 2026-06-03 · DONE.

Accounts lived only in the in-memory repo, so create+login worked but vanished
on restart. Worse, `main.cpp` wired the auth use-cases to the in-memory
`account_repo` regardless of the `[persistence]` backend, so even selecting a
durable backend would not have persisted accounts.

- **main.cpp**: select the account repository by backend — `backend="file"` →
  `infra::file::FileAccountRepository(<data-dir>)` (accounts stored as
  `<data-dir>/<name>.plain`), else in-memory. The single instance is shared by
  `BnetdService` and the auth use-cases so create/login/join see one store.
  Linked `pvpgn_infra_file` into bnetd.
- **New e2e** `tests/e2e/account_persistence_test.py`: create over the wire on
  bnetd #1 → assert the `.plain` file exists on disk → restart bnetd #2 in the
  same workdir → login authenticates against the reloaded account (0x00), with
  an unknown-user negative control (0x01). Registered as ctest
  `e2e.account_persistence` and added to the `check-all` gate.

The wire/harness helpers are now shared (`spawn_bnetd(..., backend=)`), and the
inmemory journey is unchanged. `check-all` green.

### Step 1.8 — hostile-input robustness e2e

**Date:** 2026-06-03 · DONE.

The three crashes found earlier all came from *well-formed* traffic, so
malformed input deserved its own guard. `tests/e2e/hostile_input_test.py` fires
15 hostile byte sequences at a real bnetd — each on a fresh connection —
truncated/zero/oversize headers, unknown SIDs, bodies too short for their
fields, a PING with no payload, out-of-order LOGONRESPONSE2, a non-0xFF first
byte, an unknown-SID flood, trailing garbage — and asserts bnetd never crashes
and still serves a clean AUTH_INFO handshake afterward. It asserts only "stays
up" and "still serves" (a reply, a clean close, or a silent drop are all fine).

Result: bnetd survives all 15 (the framing/dispatch/decode pipeline is robust);
no new bugs, and the path is now regression-guarded. Registered as ctest
`e2e.hostile_input` and added to `check-all` (now 15/0/0). Under the asan/ubsan
presets this doubles as a sanitizer target for the inbound pipeline.

## Milestone 2 — finish the strangler (in progress)

### Step 2.1 — delete the bnetd `LegacyBridge` glue

**Date:** 2026-06-03 · DONE, build-verified.

First strangler removal under the M1 safety net. The `app_bnetd_legacy_bridge`
static library carried two TUs: `asio_event_loop.cpp` (**live** — `main.cpp`
constructs an `AsioEventLoop`) and `legacy_bridge.cpp` (**dead**). `LegacyBridge`
was the interleaving shim between the old `select()`-based legacy `server_run()`
loop and the v3 Asio `io_context`; its real implementation compiled only under
`PVPGN_V3_BNETD_INTEGRATION`, a macro **removed from the production build in
Phase 3 and never defined since** — so `main.cpp`'s `LegacyBridge::init()` /
`::shutdown()` calls resolved to inline no-op stubs, and the legacy loop they
bridged to no longer exists. The only thing keeping the class alive was a unit
test that *artificially* defined the macro to exercise the dead code.

Removed:
- `src/app/bnetd/include/app/bnetd/legacy_bridge.hpp` +
  `src/app/bnetd/src/legacy_bridge.cpp` (`git rm`).
- The `app_bnetd_legacy_bridge` CMake library target; `asio_event_loop.cpp`
  now compiles directly into the `bnetd` executable (its deps — Boost::system,
  application_auth/connection, domain_connection — were already on `bnetd`).
- The two no-op `LegacyBridge` calls + the include from `main.cpp`.
- The three dead-singleton test cases (`instance() throws` / `tick()` /
  `init+tick+shutdown lifecycle`) and the `PVPGN_V3_BNETD_INTEGRATION=1`
  test-only compile-def. The genuinely-useful multi-thread drain test was
  **converted** to the live `AsioEventLoop::run_for` API so `AsioEventLoop`
  coverage is preserved, not lost.

Proof of no behaviour change: the calls deleted were no-ops, so runtime is
identical by construction; `check-all` is **15 passed / 0 failed / 0 skipped**
(incl. the three e2e journeys — modern login, account persistence, hostile
input — driven against the rebuilt `bnetd`), and `ctest -L unit` is **2568/2568**
(was 2571 in this build config; −3 dead tests). Layering allow-list stays empty.

Remaining `legacy_*` in `src/` after this step: 17 files (was 19) — next
candidates: `infra/legacy_config` (live config adapter — needs a replacement
before removal, not pure deletion), the `app/{d2cs,d2dbs}/legacy_*_bridges`
header dirs, `infra/clock/legacy_clock_bridge`, the scripting Lua compat shims,
and `core/legacy_compat.hpp`. Note: `protocol/bnet/{messages_legacy.hpp,
codec/codec_legacy_ols.cpp}` are **not** strangler debt — they implement the
OLS wire protocol Step 1.5 wired and must stay.

### Step 2.2 — delete the dead `infra/clock` `LegacyClockBridge`

**Date:** 2026-06-03 · DONE, build-verified.

`LegacyClockBridge` adapted the legacy global `extern time_t now;` (from the old
`src/bnetd/server.h`) to the v3 `IClock` interface. But `src/bnetd/` was deleted
long ago, so that global has **no definition anywhere** in the tree, and nothing
in production constructs the bridge — the real clocks (`SystemClock` /
`ManualClock` / `IClock`) live in `core` and are exercised by `test_core_clock`
(registered + passing). Decisively, `src/infra/clock/CMakeLists.txt` is **never
`add_subdirectory`'d** from `src/CMakeLists.txt`, so the `infra_clock` target
isn't even created — and `tests/unit/infra/clock` is gated behind
`if(TARGET infra_clock)`, which is always false. The whole subtree was
unreachable, uncompiled code.

Removed:
- `src/infra/clock/` (the `CMakeLists.txt` + `legacy_clock_bridge.{hpp,cpp}`).
- `tests/unit/infra/clock/` (the gtest that only duplicated the core clock tests
  plus a `LegacyClockBridgeCompiles` case for the dead bridge).
- The dead `if(TARGET infra_clock) add_subdirectory(clock) endif()` guard in
  `tests/unit/infra/CMakeLists.txt`.

Proof of no behaviour change: the code was never built, so production is
byte-identical; reconfigure + rebuild clean, `check-all` **15/0/0** (all three
e2e journeys included), unit suite green. `legacy_*` in `src/`: 17 → 15.

Remaining `legacy_*` candidates: the `app/{d2cs,d2dbs}/legacy_*_bridges` header
trees are **not** quick deletions — they're the *live* migrated d2cs/d2dbs
implementation (56 / 18 `.cpp` includers), so retiring them is a de-bridge/rename
job, not a strip. The scripting Lua compat shims (`infra/scripting/legacy_shim`,
`infra/scripting/lua/legacy_compat_shim` → `core/legacy_compat.hpp`) form one
Lua-gated chain. `infra/config/{legacy_prefs,legacy_ini_notice,*_legacy_prefs}.hpp`
and `infra/legacy_config` are live config adapters.

### Step 2.3 — delete three unused legacy compat shims (YAGNI sweep)

**Date:** 2026-06-03 · DONE, build-verified **with Lua ON**.

Static analysis found three "legacy compat" surfaces with **zero consumers**
anywhere in the tree — deleted per the plan's "delete speculative code on sight"
rule (README operating rule 4 / [01-principles.md](01-principles.md) YAGNI):

| Removed | Symbol | Why dead |
|---------|--------|----------|
| `core/legacy_compat.hpp` | `core::compat::xalloc`/`xstrdup`/… | internal xalloc→STL **migration aid**; 0 `#include`s, 0 `core::compat::` users — the migration it assisted is finished |
| `infra/scripting/legacy_shim.{hpp,cpp}` | `install_legacy_shim` (bnetd_*→pvpgn.* Lua globals) | defined, **never called** — never installed into any `sol::state` |
| `lua/legacy_compat_shim.{hpp,cpp}` | `LegacyCompatShim::setup_legacy_api` (legacy t_account/t_connection Lua API) | defined, **never called** — no runtime wiring |

User chose the full sweep over keeping the two Lua shims as a dormant
backward-compat feature (they had no install site, so they protected nothing).

Also removed the now-dangling build references:
- `src/legacy_shim.cpp` from `infra/scripting/CMakeLists.txt` SOURCES.
- `src/legacy_compat_shim.cpp` from `infra/scripting/lua/CMakeLists.txt` SOURCES.
- `core/legacy_compat.hpp` from the `test_core_headers_selfcontained` HEADERS
  list in `tests/unit/core/CMakeLists.txt` (the per-header self-contained
  compile-check is an explicit list, not a glob — it tried to `#include` the
  deleted header and was the one build break this step produced; fixed).

Build-verified on the **Lua-ON** `v3-dev` configure (where both scripting TUs
actually compile): reconfigure + rebuild clean, `check-all` **15/0/0** (3 e2e
journeys included), `ctest -L unit` **2568/2568** (unchanged — the shims had no
tests). `legacy_*` in `src/`: 15 → **10**.

The remaining 10 are all **live**, not strangler debt: the d2cs/d2dbs
`legacy_*_bridges` trees (migrated implementation, a de-bridge/rename job), the
`infra/config` legacy-prefs/ini adapters + `infra/legacy_config` loaders (parse
real legacy `.conf` dialects), and `protocol/bnet/{messages_legacy.hpp,
codec_legacy_ols.cpp}` (the OLS wire protocol wired in Step 1.5). M2's
quick-deletion phase is essentially exhausted; what's left is rename/de-bridge
work, not removal.

## Milestone 1 (revisited) — coverage measurement

### Step 1.9 — measure domain+app coverage: 57%, and *why* (a real wiring bug)

**Date:** 2026-06-03 · DONE (measurement); FINDINGS below.

Built the `v3-coverage` preset (`--coverage`), ran the suite to emit `.gcda`,
and ran `check-coverage.sh … 85`:

```
Coverage (domain + application): 57.00% (~6240/10949 lines)
Floor: 85%  → FAIL
```

So the M1 exit's "coverage ≥ 85% on domain+app" was **assumed but never
measured** in prior steps — it is actually **57%**. Two distinct causes, one of
them a genuine bug, not just thin tests:

**(A) A structural test-wiring bug silently drops 17 application use-case tests.**
There are *two* parallel definitions of the application libraries:
- a **modular** tree `src/application/*/CMakeLists.txt` (one lib per package,
  named `pvpgn_application_social`, `pvpgn_application_ladder`, …), driven by
  `src/application/CMakeLists.txt` — which is **never `add_subdirectory`'d** from
  `src/CMakeLists.txt`, so all of it is dead;
- the **monolith** `src/CMakeLists.txt`, which re-declares most of those libs
  under different names (`application_social` @1196, `application_moderation`
  @1175, `application_realm` @999).

The test tree straddles the two naming schemes. `tests/unit/application/CMakeLists.txt`
guards `if(TARGET pvpgn_application_social)` (line 50) and
`if(TARGET pvpgn_application_ladder)` (line 53) — **targets that never exist** —
and the social/ladder test `CMakeLists.txt`s `DEPS` against those same dead
names. Net effect: **14 social + 3 ladder use-case test files never build or
run**, in *any* preset (confirmed: `test_application_social_*` absent from both
`v3-dev` and `v3-coverage` ctest lists; `add_friend.cpp.gcno` exists but has no
`.gcda`). These map directly onto the worst 0%-covered files
(`add_friend/create_clan/invite_to_clan/…`).

Worse for **ladder**: the monolith has **no** `application_ladder` target and no
ladder gcno at all — the ladder use-case sources
(`get_ladder_entry/get_ladder_page/recompute_ladder.cpp`) are **compiled
nowhere**; the layer is entirely unbuilt.

*(moderation @1175 and realm @999 use the correct `application_*` names with
matching guards, so those tests do run — their 0%-covered files are a thinner,
separate gap, not a drop.)*

**(B) Pre-existing parallel-isolation flakes** (not my changes): under high
`-j`, 1–4 file-based loader tests in `infra/legacy_config` /
anongame-maplists / multilocale fail nondeterministically (different set each
run; **all pass when re-run `-j1`**). They share fixture paths/cwd. Matches the
known `adopt_native_handle` `-j` quirk noted in memory. `.gcda` is still emitted
regardless, so coverage numbers are unaffected.

**No source changed in this step** — it is a measurement + root-cause writeup.
The obvious highest-impact fix (re-point the social/ladder test wiring at the
real targets, and decide the monolith-vs-modular duplication for ladder) is its
own step, pending a dialog decision on scope.

### Step 1.10 — un-drop the social use-case tests (wiring fix) + one real bug

**Date:** 2026-06-03 · DONE, build-verified. Coverage **57.00% → 59.77%**.

Acted on the Step 1.9 finding. Re-pointed the mis-wired application tests at the
real monolith targets and fixed the fallout:

- **`tests/unit/application/CMakeLists.txt`**: guards `pvpgn_application_social`
  → `application_social`, `pvpgn_application_ladder` → `application_ladder`.
- **`tests/unit/application/social/CMakeLists.txt`**: 14 test `DEPS`
  `pvpgn_application_social` → `application_social`.
- **`tests/unit/application/ladder/CMakeLists.txt`**: `DEPS` → the real
  `application_ladder`/`domain_ladder`/`domain_identity` names.
- **`src/CMakeLists.txt`**: completed the monolith `application_social` lib with
  its 4 sources that compiled nowhere (`join_clan`, `leave_clan`, `create_team`,
  `disband_team`).
- **Real bug fixed** — `infra/inmemory/friend_list_repository.hpp` used
  `std::unique_lock` but included only `<shared_mutex>` (which declares
  `shared_lock`/`shared_mutex`, *not* `unique_lock` — that's in `<mutex>`).
  Added `#include <mutex>`. This header compiled nowhere before (only the
  dropped friends tests reached it), so the error was latent.

Result: **10 clan/team social use-case test files (~71 new test cases) now build
and run**; suite **2568 → 2639**, 100% green; `check-all` **15/0/0**; coverage
+2.77 pts (the 4 newly-compiled social sources also enlarge the denominator,
10949 → 11443 lines).

**Two things turned out rotted and were deferred (build-green via exclusion):**

1. **The 3 "friends" tests** (`add_friend`/`remove_friend`/`list_friends`) copy
   `InMemoryAccountRepository`/`InMemoryFriendListRepository` *by value*, but
   those repos became non-copyable (`shared_mutex` member) since the tests were
   written; the copies also defeat the seed-then-use shared state the tests
   rely on. Needs a fixture rewrite to share one `shared_ptr` repo — commented
   out in `social/CMakeLists.txt` with a TODO until then.

2. **The ladder use-cases don't compile at all** — the headers reference a
   removed `ports` namespace and the constructors have drifted from the tests
   (`too many initializers`). Bit-rot from compiling nowhere. The monolith
   `application_ladder` target is intentionally *not* declared (it would break
   the build); deferred to a repair step.

Both are classic "dead/uncompiled code rots" — the Step 1.9 wiring bug hid the
breakage. The honest coverage number is now **59.77%**; closing toward 85% needs
(a) the friends-test rewrite, (b) the ladder repair, and (c) net-new tests for
the genuinely thin layers (moderation/realm use-cases still near 0%).

### Step 1.11 — rewrite + re-enable the 3 "friends" tests

**Date:** 2026-06-03 · DONE, build-verified. Coverage **59.77% → 60.70%**.

Fixed the friends tests deferred in 1.10. Root cause: they constructed the
use-case repos with `std::make_shared<InMemoryXRepository>(value_repo)` — a
*copy* of a value repo — which (a) no longer compiles (the repos hold a
`shared_mutex`, so they're non-copyable) and (b) was already a latent logic bug:
the use-case got a fresh copy, not the instance the test seeded.

- **`remove_friend_test.cpp`**: fixture now holds `shared_ptr` repos and seeds
  through them (`friend_lists->save`), passing the same instances to the
  use-case.
- **`add_friend_test.cpp`**: the per-test repos were value locals copied into
  `make_shared`; replaced the dead `Fixture` with two small helpers
  (`make_accounts({{id,name},…})` building a seeded `shared_ptr` account repo,
  and `make_uc(accounts)`), and rewrote the 4 cases to use them.
- **`list_friends_test.cpp`**: both cases now build `shared_ptr` account +
  friend-list repos and seed through them.
- Re-enabled all three in `social/CMakeLists.txt` (removed the 1.10 TODO block).

Result: **9 friends test cases now run** (4 add + 3 remove + 2 list); whole
suite green, `check-all` **15/0/0**, coverage +0.93 pts. All 13 social use-case
test files are now wired (was 0 before 1.10).

*(The 2 intermittent failures seen in the coverage `-j` run are the same known
anongame-loader parallel-isolation flakes from 1.9 — both pass on `-j1`.)*

**Still open toward the 85% floor:** (a) the **ladder** repair (use-cases don't
compile — removed `ports` namespace + drifted constructors); (b) net-new tests
for genuinely thin layers (moderation/realm use-cases near 0%, several
application FSMs). 60.70% is the honest current number.

### Step 1.12 — re-enable the 5 disabled moderation use-case tests

**Date:** 2026-06-03 · DONE, build-verified. Coverage **60.70% → 61.18%**.

Of 8 moderation test files, only 3 ran (`check_ip_ban`, `ban_ip`,
`kick_connection`); the other 5 were commented out in
`tests/unit/application/moderation/CMakeLists.txt` with TODOs. Two root causes,
both now fixed:

1. **Stale fakes** — `ban_account`, `silence_user`, `issue_warning` each declared
   a `FakeAccountRepository` against the *old* `IAccountRepository` port
   (`find_by_name(std::string_view)`, `find_by_id(uint32_t)`, `exists`,
   `list_online`, `count`, non-const). Migrated all three to the current port
   (`find_by_id(domain::AccountId) const`, `find_by_name(const domain::UserName&)
   const`, `save`/`remove(domain::AccountId)` → `core::Status<>`, `forEach`,
   `size`), preserving each test's `account_exists`/`save_fails` toggles, and
   added the `<cstddef>`/`<functional>` includes the migrated bodies need.

2. **Sources compiled nowhere** — the monolith `application_moderation` lib
   (`src/CMakeLists.txt`) omitted `issue_warning.cpp` and `list_bans.cpp`, so
   those use-cases linked nowhere (the "static-link ordering" TODO on
   `list_bans` was really just the missing source). Added both to the lib's
   SOURCES. `unban_account`/`list_bans` tests use only ban/ip/event fakes (no
   account-repo drift), so they needed no fake migration.

Re-enabled all five (`ban_account`, `unban_account`, `silence_user`,
`issue_warning`, `list_bans`). Result: **21 moderation use-case cases now run**
(was a handful); suite **2648 → 2669**, `check-all` **15/0/0**, coverage
+0.48 pts. (The 2 intermittent coverage-run failures remain the known
anongame-loader `-j` flakes; pass `-j1`.)

Note: these moderation use-cases are not yet wired into any protocol handler
(`BanAccount` etc. have 0 protocol/app references) — the tests pin their
behaviour as real application logic ahead of wiring, the same shape as the
social use-cases.

### Step 1.13 — repair + wire the ladder use-cases (the 1.10 deferral)

**Date:** 2026-06-03 · DONE, build-verified. Coverage **61.18% → 61.55%**.

Closed the ladder deferral from 1.10. Three layers of rot, all from the
use-cases compiling nowhere (the orphaned modular `src/application/ladder` tree):

1. **Header namespace rot** — `recompute_ladder.hpp` / `get_ladder_entry.hpp` /
   `get_ladder_page.hpp` referenced a removed `ports::` namespace for the repo
   interfaces. The ports had moved: `ILadderRepository` → `domain::ladder`,
   `IAccountRepository` → `domain::identity`. Repaired the 3 headers to the
   real namespaces + added the missing `#include`s. (The `.cpp` bodies already
   qualified `domain::ladder::`/`domain::identity::`, so they were fine — and
   the test's `RecomputeLadder uc{repo}` was correct all along; the earlier
   "too many initializers" was a cascade from the broken `ports::` type.)
2. **Compiled nowhere** — declared the monolith `application_ladder` lib in
   `src/CMakeLists.txt` (3 sources, deps `core`/`domain_ladder`/`domain_identity`)
   and re-enabled the test guard; migrated the two ladder tests' own
   `FakeAccountRepository` fakes to the current port (same drift as moderation).
3. **A real test-logic bug** — `RecomputeLadder: happy path` asserted
   `repo.entries[]` ends up in rank order, but the fake's `save_entry` updated
   entries *in place* by account, so it never reordered. The use-case sorts a
   *copy* and re-saves in rank order; fixed the fake's `save_entry` to
   erase-then-append so stored order reflects save order (= rank). Now the
   assertion verifies real behaviour.

Result: `application_ladder` compiles + links; **all 3 ladder use-case test
files run** (recompute/get_entry/get_page); suite **2669 → 2677**, `check-all`
**15/0/0**, coverage +0.37 pts (ladder sources now counted, 11909 → 12132
lines). Both 1.10 deferrals (friends + ladder) are now closed.

**Coverage status:** **61.55%** vs the 85% floor. The remaining gap is genuine
under-testing (the realm use-cases are the next 0% cluster; several application
FSMs and infra adapters are thin), not more dropped/rotted tests — the
test-wiring archaeology that started at 1.9 is essentially exhausted.

### Step 1.14 — re-enable the 3 disabled realm use-case tests

**Date:** 2026-06-03 · DONE, build-verified. Coverage **61.55% → 62.29%**.

Realm had 14 sources (all compiled) and 14 tests, but 3 were commented out:

1. **`character_lock_test`** — constructed `domain::realm::Character(id, stats)`
   with the old 2-arg ctor; current ctor is `(id, stats, core::SystemTime)`.
   Added the `core::SystemTime{}` arg (2 sites) + `core/clock.hpp`.
2. **`register_realm_test`** — the TEST_CASEs (at global scope, after the
   `namespace pvpgn::application::realm` fake) used bare `core::StatusCode`,
   which doesn't resolve there. Added a `namespace core = pvpgn::core;` alias
   beside the existing `pa` alias. (The "needs a not-yet-present
   realm_repository port" TODO was stale — `domain::realm::IRealmRepository`
   already exists and the fake implements it correctly.)
3. **`character_persistence_test`** — the use-case validates save data through
   `D2SaveCodec::parse` (signature `0xAA55AA55` + version + `MIN_FILE_SIZE`
   335 bytes), but the 3 asserting tests fed 100 zero bytes, so `save()`
   rejected them. Added a `make_valid_save()` helper (335-byte parse-valid blob)
   and used it in `SaveCharacter`/`SaveAndLoadCharacter`/`CharacterExists`;
   tightened `CharacterExists` to actually `REQUIRE` the save succeeds.
   (`load()` takes `char_name` from the command and the codec extractors only
   read bytes, so one parse-valid blob satisfies all three.)

Result: **all 14 realm use-case test files run** (+~27 cases); suite
**2677 → 2696**, `check-all` **15/0/0**, coverage +0.74 pts.

**Coverage tally for the M1 test-wiring repair arc (1.9–1.14):** 57.00% →
**62.29%** (+5.3 pts) by un-dropping/repairing **social (13) + ladder (3) +
moderation (5) + realm (3) = 24 use-case test files** that the build silently
skipped, plus fixing ~5 latent bugs (the `friend_list_repository` `<mutex>`,
the `RecomputeLadder` fake ordering, the ladder header `ports::` rot, the
missing `application_ladder`/`issue_warning`/`list_bans` lib sources). The
"dropped/rotted tests" backlog is now exhausted; further climb to 85% is
net-new tests for thin-but-wired areas (e.g. `permission_checker`, application
FSMs, infra adapters) — a different kind of work.

### Step 1.15 — net-new tests for the application `PermissionChecker`

**Date:** 2026-06-03 · DONE, build-verified. Coverage **62.29% → 63.04%**.

First *net-new* test step (the dropped/rotted backlog being exhausted at 1.14).
`application/auth/src/permission_checker.cpp` (the `InMemoryPermissionChecker`
use-case that maps an account's command groups → moderation `Permission` sets,
backed by `IAccountRepository`) had **0%** coverage: the existing
`permission_checker_test.cpp` tests the *infra* `InMemoryPermissionChecker`
(a flat grant store), a different class — the application use-case had no test.

Added `tests/unit/application/auth/permission_checker_application_test.cpp`
(8 cases) over a real `infra::inmemory::InMemoryAccountRepository`: admin
(group 1) gets admin-only perms (ShutdownServer/BanUser); mod (2) gets
moderation perms but is denied admin ones; operator (3) manages channels only;
voice (4) gets only SetChannelTopic; no-groups → nothing; unknown account →
denied (both `has_permission` and `has_command_group`); combined groups (3+4)
union their permissions; `has_command_group` reflects membership.

Result: `permission_checker.cpp` **0% → 97.44%** (of 78 lines); suite
**2696 → 2704**, `check-all` **15/0/0**, overall coverage +0.75 pts.

**Coverage now 63.04%.** The remaining climb to the 85% floor is more net-new
tests across thin-but-wired areas (other application FSMs, infra adapters); the
check-all `--deep` coverage gate label says ">=85%" but currently passes its
arg-defaulted 70% floor — raising that floor toward the real number (with a
documented ramp) is a candidate follow-up so the gain can't silently regress.

### Step 1.16 — enforce the coverage gain (ratchet) + fix the bench gate

**Date:** 2026-06-03 · DONE, build-verified. `check-all --deep` **17/0/5**.

Locked in the 1.9–1.15 coverage gains and fixed two `check-all --deep` gate bugs
the gains exposed:

1. **Coverage gate was mislabeled and unenforced.** The `--deep` gate printed
   ">=85%" but invoked `check-coverage.sh` with no floor arg → defaulted to 70.
   Since real coverage is **63.04%**, the gate would actually *fail* at 70 (it
   had simply never run, ring-3 being opt-in). Set an explicit **no-regress
   ratchet floor of 62%** (just below current) with the label
   `coverage (>=62% domain+app, ramp->85)` and a comment: raise toward the 85%
   M1 exit as net-new tests land, never lower. Verified PASS at 63.04%.
2. **Bench gate was a guaranteed FAIL.** It called
   `check-bench-regression.py` with **no args**, but the script requires
   `<baseline> <current>` → argparse usage error. Wired the committed
   host baseline (`tests/bench/baselines/local-gcc13.json`) vs the generated
   `bench-results.json`, and the presence-guard now checks the baseline too.
   Verified PASS (all benchmarks within the 10% budget).

`check-all --deep` now reports **17 passed / 0 failed / 5 skipped** (honest
skips: asan/ubsan/tsan/mutation/fuzz — those build dirs/runtimes absent). The
ring-2 default remains 15/0/0.

**Session close-out (2026-06-03):** this run did M2 Steps 2.1–2.3 (3 dead-
strangler deletions, `legacy_*` 19→10) and the M1 coverage arc 1.9–1.16
(measure → repair 24 silently-dropped use-case test files → first net-new tests
→ ratchet the floor). Domain+app coverage **57.0% → 63.04%**; ~6 latent bugs
fixed; tree green at every commit. Remaining toward M1 exit: keep ramping the
coverage floor with net-new tests (target 85%), then M3+.

### Step 1.17 — net-new tests for AttributeMap timestamps + parse fallbacks

**Date:** 2026-06-03 · DONE, build-verified. Coverage **63.04% → 63.47%**.

`domain/identity/src/attribute_map.cpp` was 49% — the existing
`attribute_map_typed_test` covers the email/sex/location/description accessors
and the win/loss/disconnect increments, but **not** `username()`,
`last_login()`/`created_at()` (parse + round-trip + the malformed-value `catch`
paths), nor the stat getters' non-numeric fallbacks. Added
`attribute_map_timestamps_test.cpp` (7 cases): `username()` raw-key read;
`last_login`/`created_at` round-trip through their setters; malformed timestamps
→ `nullopt` (catch); all five stat getters → 0 on non-numeric values (catch);
ladder increments accumulate independently of the win/loss counters.

Result: `attribute_map.cpp` **49.1% → 86.21%** (of 116 lines); suite
**2704 → 2710**, `check-all` **15/0/0**, overall coverage +0.43 pts → **63.47%**.

**Targeting note:** verified the per-file picks are *real* (computed the
max-coverage-per-file aggregate across all `.gcno` to rule out the multi-link
first-occurrence artifact that makes value-object *headers* like `ip_address.hpp`
read as 0% despite being tested). The genuine remaining low-coverage `.cpp`
cluster is the **application connection FSMs** (`connection_fsm_inchannel` 30%,
`_authenticating` 33%, `_connecting` 47%) — the biggest uncovered-line sink, but
FSM-driving tests are more involved than pure value-object/use-case tests.

### Step 1.18 — connection-FSM InChannel branch coverage (partial; ROI finding)

**Date:** 2026-06-03 · DONE, build-verified. Coverage **63.47% → 63.55%**.

Targeted the biggest uncovered-line sink, `connection_fsm_inchannel.cpp` (232
lines, 30%). Added `connection_fsm_inchannel_branches_test.cpp` (8 cases) over
the existing `connection_fsm_test_fixtures.hpp`: the `game_type` switch arms for
both `on_start_game` (STARTADVEX) and `on_join_game` (GETADVLISTEX) — 1→FreeForAll,
2→OneOnOne, 3→Cooperative, 4→Custom, other→Melee (the existing tests only used
the default); the empty-channel-name early return in `on_join_channel`; and the
out-of-order rejection of StartGame/JoinGame.

**ROI finding:** this moved `inchannel.cpp` only **30.2% → 33.19%** (+3 pts,
~7 lines). The bulk of the file's uncovered code is the **injected chat-use-case
paths** (`on_join_channel`/`on_chat_command`/`on_leave_channel` each have a big
`if (use_case_ != nullptr) { … }` block reached only when a real
`application::chat::JoinChannel`/`PostMessage`/`LeaveChannel` is wired via
`set_join_channel(...)` etc.). The existing FSM tests — and these — exercise only
the stub fallbacks. Covering the injected paths needs constructing those chat
use-cases with in-memory channel/account repos: a meaningful scaffolding
sub-project, not a quick branch sweep. Logged here so the next session can scope
it deliberately rather than rediscover it.

Result: suite **2710 → 2715**, `check-all` **15/0/0**, overall coverage +0.08
pts → **63.55%** (ratchet floor stays 62%).

### Step 1.19 — connection-FSM injected chat-use-case paths (the scaffolding)

**Date:** 2026-06-03 · DONE, build-verified. Coverage **63.55% → 64.44%**.

Built the scaffolding flagged at 1.18 and it delivered the FSM gain the branch
sweep couldn't. Added `connection_fsm_inchannel_usecase_test.cpp` (4 cases) that
wires **real** `application::chat::{JoinChannel,PostMessage,LeaveChannel}` over
in-memory channel/account/session repos, injects them via
`set_join_channel(...)`/`set_post_message(...)`/`set_leave_channel(...)`, and
drives the InChannel handlers through their real (non-stub) blocks:

- **JoinChannel success** — seed the stub-login placeholder account (`account_id_
  == 1`), dispatch SID_JOINCHANNEL → the use-case creates + persists the channel,
  admits the member, and the FSM emits EID_CHANNEL + EID_SHOWUSER + EID_JOIN
  (asserted via ≥2 chat-event packets + the channel now resolving by name).
- **JoinChannel failure** — *don't* seed the account → `AccountNotFound` → the
  FSM's `if (!result)` EID_ERROR branch (exactly one chat-event packet).
- **PostMessage** — join, then SID_CHATCOMMAND → the real PostMessage path echoes
  EID_TALK.
- **LeaveChannel** — join, then SID_LEAVECHAT → real LeaveChannel path,
  InChannel→LoggedIn.

Result: `connection_fsm_inchannel.cpp` **33.19% → 50.43%** (+17 pts — vs the
+3 the 1.18 branch sweep got); suite **2715 → 2719**, `check-all` **15/0/0**,
overall coverage +0.89 pts → **64.44%** (the biggest single-step gain since the
1.9–1.14 repair arc). **Ratcheted the no-regress floor 62 → 64.**

This validates the injection pattern as the high-ROI lever for the remaining FSM
gap: the same scaffolding (in-memory repos + real use-cases injected) applies to
the still-thin `connection_fsm_authenticating` (33%), `_connecting` (47%),
`_loggedin`, and `_ingame` handlers, and to the rest of `inchannel`'s error/
event-drain branches.

### Step 1.20 — connection-FSM injected LoginUserNls (SRP) paths

**Date:** 2026-06-03 · DONE, build-verified. Coverage **64.44% → 64.86%**.

Applied the 1.19 injection recipe to the next-thinnest FSM,
`connection_fsm_authenticating.cpp` (33%). Added
`connection_fsm_authenticating_usecase_test.cpp` (3 cases) wiring a real
`application::auth::LoginUserNls` over an in-memory `INlsCredentialStore` (seeded
with a genuine SRP salt+verifier via `infra::crypto::NlsVerifier::create_verifier`)
and the production `NlsCryptoAdapter`, constructed through the FSM's NLS ctor
(`ConnectionFsm{ctx, nls}`):

- **challenge path** — SID_AUTH_ACCOUNTLOGON for the known account → the FSM
  calls `LoginUserNls::challenge()` and replies with salt + server public key.
- **unknown account** — logon for an unseeded name → lookup failure handled, not
  authenticated.
- **verify failure** — challenge, then SID_AUTH_ACCOUNTLOGONPROOF with a canned
  (incorrect) proof → `verify()` rejects, FSM stays out of LoggedIn.

(The verify *success* path needs a real client-side SRP M1 — the same limitation
the standalone `login_user_nls_test` documents — so it stays uncovered; the
challenge + reject paths are the bulk.)

Result: `connection_fsm_authenticating.cpp` **33.33% → 83.33%** (+50 pts); suite
**2719 → 2722**, `check-all` **15/0/0**, overall coverage +0.42 → **64.86%**
(floor stays 64).

### Step 1.21 — connection-FSM injected LoginUser (OLS) path

**Date:** 2026-06-03 · DONE, build-verified. Coverage **64.86% → 65.22%**.

Third FSM via the injection recipe: `connection_fsm_connecting.cpp` (47%), whose
`on_logon_request` (legacy single-step OLS `SID_LOGON_REQUEST` 0x29) has an
injected `application::auth::LoginUser` block. Added
`connection_fsm_connecting_usecase_test.cpp` (3 cases) building a real `LoginUser`
over in-memory account/session/event-bus repos + `SystemClock` (and a stub
`LoginUserNls`, since the FSM's OLS path is only reachable through the
OLS+NLS ctor):

- **valid login** — seed account "bob" with a default (all-zero) `BNHash`, which
  matches the all-zero hash `make_logon_request` sends → `LoginUser::execute`
  succeeds → LoggedIn, and `account_id() == 7` (the real id, proving the
  use-case ran rather than the placeholder).
- **wrong password** — seed a non-zero stored hash → mismatch → 0x01 reply, not
  authenticated.
- **unknown account** — no seed → lookup fails → not authenticated.

Key gotcha (fixed): `on_logon_request` only runs in the **Connecting** state
(legacy OLS skips AUTH_INFO), so the test dispatches from the fresh FSM, *not*
after `reach_authenticating`.

Result: `connection_fsm_connecting.cpp` **47.06% → 81.18%** (+34 pts); suite
**2722 → 2725**, `check-all` **15/0/0**, overall coverage +0.36 → **65.22%**.
**Ratcheted the no-regress floor 64 → 65.** Three FSMs now done via injection
(inchannel 50%, authenticating 83%, connecting 81%); `loggedin`/`ingame` and the
rest of `inchannel` remain.

### Step 1.22 — connection-FSM InGame dispatch paths (→ 100%)

**Date:** 2026-06-03 · DONE, build-verified. Coverage **65.22% → 65.52%**.

`connection_fsm_ingame.cpp` (29.6%) has no injected use-cases — pure state logic.
The existing ingame test drove `on_leave_game` and called `bind_d2_character()`
*directly*, but never dispatched the two SID handlers that parse a packet:
`on_d2_char_select` (SID_D2GAMELISTEX 0x68) and `on_warcraft_general`
(SID_WARCRAFTGENERAL 0x44, WAR3 route token). Added
`connection_fsm_ingame_dispatch_test.cpp` (5 cases):

- D2 char-select binds class/level/name (asserted via `d2_char_*()` accessors);
  short payload (<3 bytes) is ignored; the packet is ignored while Connecting.
- WAR3 general stores the route token (`war3_route_token()`); short payload
  (<5 bytes) sets none.

Result: `connection_fsm_ingame.cpp` **29.63% → 100%**; suite **2725 → 2730**,
`check-all` **15/0/0**, overall coverage +0.30 → **65.52%** (floor stays 65).

Connection-FSM file tally now: `loggedin` 100%, `ingame` 100%, `authenticating`
83%, `connecting` 81%, `inchannel` 50%, core `connection_fsm.cpp` ~52%. The
biggest remaining FSM gaps are `inchannel`'s residual error/event-drain branches
and the core dispatch file.

### Step 1.23 — net-new tests for the LoginUser session-hash overload

**Date:** 2026-06-03 · DONE, build-verified. Coverage **65.52% → 65.85%**.

`login_user.cpp` was 40% — the existing `login_user_test` covers
`execute(LoginRequest)` (the OLS path) well, but `execute(LoginWithSessionHashRequest)`
(the ~75-line W3 session-hash path) had **no test**. Added
`login_user_session_hash_test.cpp` (4 cases) with a deterministic `FakeHasher`
(implements the one-method `IPasswordHasher`, derives the session hash from
ticks+sessionkey so the test can reproduce the expected proof):

- **missing hasher** → `Internal` (the 4-arg `LoginUser` ctor leaves `hasher_`
  null);
- **unknown user** → `UnknownUser`;
- **wrong proof** → `InvalidCredentials`;
- **correct proof** (computed via the same hasher) → authenticates, returns the
  account id.

Result: `login_user.cpp` **40.24% → 76.83%** (+37 pts); suite **2730 → 2734**,
`check-all` **15/0/0**, overall coverage +0.33 → **65.85%** (floor stays 65).
(Residual: the second overload's Banned / PersistenceFailed branches, which need
a banned-account or failing-repo fixture.)

### Step 1.24 — net-new tests for the ChangePassword session-hash overload

**Date:** 2026-06-03 · DONE, build-verified. Coverage **65.85% → 66.17%**.

Same shape as 1.23: `change_password.cpp` (42.5%) — the existing
`change_password_test` covers `execute(ChangePasswordRequest)` but not the
session-hash overload `execute(ChangePasswordWithSessionHashRequest)`. Added
`change_password_session_hash_test.cpp` (5 cases) with the deterministic
`FakeHasher`: missing hasher → `Internal`, unknown user, wrong proof →
`InvalidCurrentPassword`, no-op rotation (new == current) → `PasswordUnchanged`,
and the happy path (proof computed via the hasher) → rotates and returns the id.

Result: `change_password.cpp` **42.5% → 95.0%**; suite **2734 → 2739**,
`check-all` **15/0/0**, overall coverage +0.32 → **66.17%**. **Ratcheted the
no-regress floor 65 → 66.**

### Step 1.25 — cover the remaining chat_event_compose message-type arms

**Date:** 2026-06-03 · DONE, build-verified. Coverage **66.17% → 66.49%**.

`chat_event_compose.cpp` (65%) is a pure 16-arm switch over `LegacyMessageType`;
`chat_event_compose_test` covered 11 arms. Extended it with the 5 missing ones
(7 cases): `Part` (username-only, empty text) + its me==NULL reject; `Broadcast`
(full fields) + its MF_X reject; `UserFlags` (text from playerinfo); `WhisperAck`
(full fields); `ChannelDoesNotExist` (username from chatname). Each asserts the
mapped `event_id`, `username`, and `text`.

Result: `chat_event_compose.cpp` **65.10% → 92.62%**; suite **2739 → 2746**,
`check-all` **15/0/0**, overall coverage +0.32 → **66.49%** (floor stays 66).

## Milestone 3 — Domain & application hardening (in progress)

### Step 3.1 — remove cross-context coupling + add the enforcing gate

**Date:** 2026-06-03 · DONE, build-verified. New gate; `check-all` **16/0/0**.

M3 ([04-domain-layer.md](04-domain-layer.md) §3 / DoD) requires *no* domain
context to include another context's internals — cross-context links must go
through the shared kernel or events. An audit found **2 violations** and no
enforcing check:

- `domain/ladder/d2_ladder.hpp` → `domain/realm/character.hpp` (for
  `CharacterClass`). (`d2cs` already defines its *own* `CharacterClass`, so the
  coupling was realm↔ladder only.)
- `domain/chat/ports/command_registry.hpp` → `domain/moderation/ports.hpp` (for
  `IPermissionChecker` / `Permission`).

Both fixed by promoting the shared concept into the **published kernel**
(`domain/shared`, namespace `pvpgn::domain`):

- New `domain/shared/d2_character_class.hpp` — `CharacterClass`. `realm` and
  `ladder` now both source it from shared; `realm::CharacterClass` kept as a
  `using` alias so existing realm code is untouched.
- New `domain/shared/permission.hpp` — `Permission` + `IPermissionChecker`.
  `moderation::Permission`/`::IPermissionChecker` kept as `using` aliases (so the
  many existing callers compile unchanged); `chat` now includes only
  `domain/shared/permission.hpp`. Fixed the one *forward declaration* of
  `moderation::IPermissionChecker` (in `protocol/telnet/admin_fsm.hpp`) — a
  forward-decl can't alias, so it was repointed to `domain::IPermissionChecker`.

New gate **`scripts/check_domain_cross_context.sh`** (empty allow-list, may only
shrink) wired into `check-all` Ring 2 next to domain-purity. Both new shared
headers added to the `test_domain_shared_headers_selfcontained` list.

Result: cross-context scan **clean**; `check-all` now **16 passed / 0 / 0** (the
new gate included); full suite **2746/2746**. Layering + purity allow-lists stay
empty. This ticks the M3 DoD item "no `domain/<a>` includes `domain/<b>`
internals … a grep-based check confirms it."

### Step 3.2 — globals/singletons audit (clean) + regression gate

**Date:** 2026-06-03 · DONE, build-verified. New gate; `check-all` **17/0/0**.

Audited `src/domain` + `src/application` for the M3 exit item "no global/singleton
access in domain/app". **Result: already clean** — no singleton accessors
(`instance()`/`getInstance`), no Meyers singletons, no namespace-scope mutable
globals. The only `extern`/`prefs_get_servername` matches are *comments*; the
servername is injected via `ComposeRequest`. The composition-root smoke test
(`tests/unit/services/combined/combined_composition_test.cpp`) already exists.

So there was nothing to *replace* — but the property was unguarded for the
**application** layer (`check_domain_purity.sh` runs on `src/domain` only, and
doesn't check the singleton-accessor pattern at all). Added
**`scripts/check_no_singletons.sh`** — forbids singleton accessors, singleton
call-sites, and `g_`/`s_` mutable namespace globals across `domain` +
`application` — and wired it into `check-all` Ring 2. Verified clean (no false
positives) and added as a hard gate, so the now-met M3 property can't regress.

Result: `check-all` **17 passed / 0 / 0**. M3 exit item "no global/singleton
access in domain/app" is met **and** mechanically enforced.

### Step 3.3 — ISP port audit (ADR 0012) + first reader/writer split

**Date:** 2026-06-03 · DONE, build-verified. `check-all` **17/0/0**.

Audited every `I*` port in `src/domain` + `src/application` by method count.
Findings (the fat ports): `IUnitOfWork` (13 — 3 txn + **10 repo accessors**, a
god-port), `IIpBanRepository` (8), `IConnectionContext` (7, I/O + game callbacks),
and the read+write repositories (`IAccountRepository`/`IChannelRepository`/… at
5–6). Captured the audit, the target shape, and the sequencing in
**[ADR 0012 — Port Interface Segregation](../adr/0012-port-interface-segregation.md)**
(added to `docs/index.md` for the docs-reachable gate). Key constraint recorded:
the repo ports are implemented by **env-gated backends** (mysql/postgres/sqlite/
shadow), so the splits are backend-by-backend follow-ups, not one blind sweep.

Then **landed the first split as a concrete, build-verified example** using the
backward-compatible inherit-from-both idiom:

- `domain/identity/ports.hpp`: `IAccountRepository` → `IAccountReader`
  (`find_by_id`/`find_by_name`/`forEach`/`size`) + `IAccountWriter`
  (`save`/`remove`), with `class IAccountRepository : public IAccountReader,
  public IAccountWriter`. Every implementer (`InMemory`, file, sqlite, mysql,
  postgres, shadow, all the test fakes) keeps deriving from `IAccountRepository`
  and overriding all six methods — **unchanged**.
- Narrowed the read-only `application::auth::InMemoryPermissionChecker` to depend
  on `std::shared_ptr<IAccountReader>` (it only calls `find_by_id`) — now it is
  *impossible* to misuse it for writes.

Build-verified: full suite **2746/2746**, `check-all` **17/0/0** (the split is
ABI/source-compatible, so nothing else changed). M3 DoD "ports audited for ISP"
is met; the remaining splits are tracked in ADR 0012 as scoped follow-ups.

### Step 3.4 — extend the IAccountReader narrowing to more read-only consumers

**Date:** 2026-06-03 · DONE, build-verified. `check-all` **17/0/0**.

Applied the ADR 0012 reader/writer split to the remaining read-only account
consumers (all call only `find_by_id`): `application::social::ListFriends` and
`application::ladder::{GetLadderEntry,GetLadderPage}` now take
`IAccountReader` (`shared_ptr` / `&`) instead of `IAccountRepository`. Their
constructions are unchanged (an `InMemoryAccountRepository` upcasts to
`IAccountReader`), so all call sites and tests compile untouched.

Result: 4 read-only use-cases (incl. the permission checker from 3.3) now depend
only on the account *read* surface — they cannot perform writes. Full suite
**2746/2746**, `check-all` **17/0/0**.

### Step 3.5 — IChannelRepository reader/writer split (second context)

**Date:** 2026-06-03 · DONE, build-verified. `check-all` **17/0/0**.

Applied the ADR 0012 split to a second context to show it generalizes:
`domain/chat/ports.hpp` `IChannelRepository` → `IChannelReader`
(`find_by_id`/`find_by_name`/`forEach`/`size`) + `IChannelWriter`
(`save`/`remove`), composite inherits both (implementers unchanged). Narrowed the
read-only `application::chat::ListChannels` (calls only `forEach`) to
`std::shared_ptr<IChannelReader>`.

Build-verified: full suite **2746/2746** (one anongame `-j` flake, passes `-j1`),
`check-all` **17/0/0**. The account + channel repositories now both expose
segregated read/write ports with their read-only consumers narrowed.

### Step 3.6 — domain docs DoD: already met; refresh shared-kernel doc

**Date:** 2026-06-03 · DONE.

Went to create `docs/domain/<ctx>.md` (the domain-layer DoD doc item) and
discovered it is **already satisfied**: comprehensive per-context guides exist at
`docs/developer/contexts/*.md` for all 12 contexts, linked from `docs/index.md`'s
"Bounded Contexts" section. Creating a parallel `docs/domain/` tree would be
confusing duplication — so the briefly-created copies were removed.

Instead did the real value-add: the existing `shared.md` did not yet document the
types promoted into the shared kernel by **M3.1**. Updated it to list
`CharacterClass` (`domain/shared/d2_character_class.hpp`, shared by realm+ladder),
`Permission` (`domain/shared/permission.hpp`), and a new "Cross-cutting ports"
subsection for `IPermissionChecker` — with the note that
`moderation::IPermissionChecker` is a kept alias and that cross-context coupling
is enforced by `scripts/check_domain_cross_context.sh`.

Result: domain-layer DoD "docs/domain/<ctx>.md per context" is met (at
`docs/developer/contexts/`); the shared-kernel doc now reflects the M3.1
decoupling. Ring-1 gates (incl. docs-reachable) green.

**Milestone 3 status:** cross-context coupling removed + gate (3.1); singletons
audited clean + gate (3.2); ISP audited (ADR 0012) with account+channel
reader/writer splits and 5 read-only consumers narrowed (3.3–3.5); per-context
docs confirmed/refreshed (3.6). Remaining M3: the broader ISP splits over
env-gated backends (tracked in ADR 0012) and a deeper anemic-model pass.

## Milestones 4–6

Not started. See [`plans/14-migration-roadmap.md`](../../plans/14-migration-roadmap.md).
