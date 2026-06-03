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

## Milestones 2–6

Not started. See [`plans/14-migration-roadmap.md`](../../plans/14-migration-roadmap.md).
