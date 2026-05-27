# R192 Checklist — `WITH_BNETD=ON` CI lane (AUDIT, deferred)

Started: 2026-05-27.

## Goal

Defensive CI lane to prevent recurrence of R168→R169 / R171/R172→R191.a-class
"installer declared+defined but never called" regressions, by configuring
and building `bnetd` (the legacy executable) with `WITH_BNETD=ON +
PVPGN_BUILD_V3=ON` so that:

- `PVPGN_V3_BNETD_INTEGRATION=1` is set on `bnetd_legacy` (per
  `src/bnetd/CMakeLists.txt:144`), so the v3-gated strangler blocks in
  `src/bnetd/server.cpp` + `src/bnetd/handle_*.cpp` are compiled, and
- the `integration_legacy_bnetd_linked` library is linked, surfacing
  any link-time symbol mismatches.

## Findings (pre-existing bugs uncovered)

### Finding 1 — dangling source reference

- [x] Identified `src/bnetd/CMakeLists.txt:37` references
      `handle_wserv.cpp` / `handle_wserv.h`, deleted in commit
      `03f35f9` but never removed from the source list. Configure step
      under WITH_BNETD=ON fails with `Cannot find source file:
      handle_wserv.cpp` → `No SOURCES given to target: bnetd_legacy`.
- [x] **Fixed**: removed the dangling pair from `BNETD_LIB_SOURCES`.

### Finding 2 — `prefs_v3_shim.h` calls 3 non-existent bridge functions

- [x] Identified that `src/bnetd/prefs.cpp` calls
      `pvpgn_v3_prefs_get_xplevel_file()` and `..._xpcalc_file()`
      (with underscores) which are **not declared** in
      `prefs_bridge.hpp`. The header has the no-underscore variants
      (`pvpgn_v3_prefs_get_xplevelfile`, `..._xpcalcfile`). This bug
      is dormant because `prefs.cpp` doesn't currently compile in any
      CI lane (see Finding 3).
- [ ] Defer: rename either the bridge functions or the call sites to
      match. Cosmetic compared to Finding 3.

### Finding 3 — **SYSTEMIC CMake ordering bug: `integration_legacy_bnetd_linked` is never created**

This is the actual blocker for the CI lane and the most important
finding of R192.

- **Repro**: `docker build -f Dockerfile.v3 --target v3-bnetd-on -t
  pvpgn-v3-bnetd-on:r192 .` (after fixing Finding 1) → ~80 errors in
  `prefs_v3_shim.h` of the form
  `pvpgn_v3_prefs_get_XXX was not declared in this scope`.
- **Root cause** (confirmed via inspection):
  1. Top-level `CMakeLists.txt:209-211` orders
     `add_subdirectory(src/v3)` BEFORE `add_subdirectory(src)`.
  2. The `integration_legacy_bnetd_linked` target is created in
     `src/v3/CMakeLists.txt:1352-1353` inside
     `if(TARGET bnetd_legacy AND PVPGN_V3_WITH_BOOST)`.
  3. When `src/v3` is processed, `bnetd_legacy` does NOT yet exist
     (it's created in `src/bnetd/CMakeLists.txt:63` during the LATER
     `add_subdirectory(src)`).
  4. Therefore `integration_legacy_bnetd_linked` is **never created**
     under any `WITH_BNETD=ON + PVPGN_BUILD_V3=ON` configure.
  5. Then `src/bnetd/CMakeLists.txt:122 if(TARGET
     integration_legacy_bnetd_linked)` is FALSE.
  6. So `target_compile_definitions(bnetd_legacy PRIVATE
     PVPGN_V3_BNETD_INTEGRATION=1)` at line 144 is never executed.
  7. So `prefs_v3_shim.h:25 #ifdef PVPGN_V3_BNETD_INTEGRATION` is FALSE
     and `#include "integration/legacy_bnetd/prefs_bridge.hpp"` is
     skipped. The inline shim accessor bodies still unconditionally
     call `pvpgn_v3_prefs_get_*()` → undeclared.

- **Implication**: the legacy `bnetd` executable build has been broken
  since this v3-first ordering was introduced. Every Phase-3 progress
  entry that claims `install_*_handler()` was "wired in server.cpp"
  (R168, R171, R172, R191.a) describes code that is **dead** under
  the current CMake graph — `PVPGN_V3_BNETD_INTEGRATION` is not even
  defined, so the `#ifdef` blocks containing the install_*() calls
  don't compile. The v3 docker pipeline (`WITH_BNETD=OFF`) doesn't
  touch any of this, which is why the bug has never been caught.

- **NOT a quick fix.** Multiple plausible repair strategies, each with
  its own design implications:

  A. **Move target creation**: relocate the
     `integration_legacy_bnetd_linked` block from
     `src/v3/CMakeLists.txt:1352-1450` into
     `src/bnetd/CMakeLists.txt` (right after `bnetd_legacy` is
     added). Pro: localized. Con: ~100 lines of v3 source-file
     listings + include-path scaffolding migrated into the legacy
     subdir, breaking the "v3 owns its own build wiring" invariant.

  B. **Two-pass `src/v3`**: split off
     `src/v3/integration/legacy_bnetd/CMakeLists.txt` as a separate
     subdir and add it AFTER `add_subdirectory(src)`. Top-level
     order becomes: `src/v3` (most of v3) → `src` (legacy + bnetd)
     → `src/v3/integration/legacy_bnetd` (now sees bnetd_legacy).
     Pro: minimal disruption to either tree. Con: needs the
     v3 CMakeLists internal `if(TARGET integration_...)` checks
     in OTHER subdirs to still work (they don't depend on it for
     the rebuild — verify).

  C. **CMake `cmake_language(DEFER)` / generator expressions**:
     reformulate the conditional to fire after both trees configure.
     Pro: surgical. Con: CMake 3.19+; complex to reason about.

  D. **Reverse top-level order**: `add_subdirectory(src)` first,
     then `add_subdirectory(src/v3)`. Pro: simplest one-line fix.
     Con: breaks the existing rationale in `CMakeLists.txt:200-208`
     (MSVC `remove_definitions(-DUNICODE -D_UNICODE)` before v3,
     plus v3 targets are referenced by `src/bnetd/CMakeLists.txt`
     conditional blocks). All those conditionals would silently
     become FALSE → reverse of current bug.

- [ ] Decision **deferred to human review** — pick a strategy, then
      land it as R193, then R194 = the CI lane this R192 wanted.

## Actions taken this round

- [x] Removed dangling `handle_wserv.cpp` + `handle_wserv.h` from
      `src/bnetd/CMakeLists.txt:37` (Finding 1 fix).
- [x] Added `v3-bnetd-on` stage to `Dockerfile.v3` to reproduce the
      bug — then REVERTED, because leaving a never-passing stage in
      the default Dockerfile would break any `docker build -f
      Dockerfile.v3 .` without `--target`.
- [x] Authored this checklist + a detailed audit entry in
      `plans/progress-master.md` + `changelog.md`.
- [ ] No fix to Finding 3 — out of scope for one autonomous round
      given the architectural implications.

## Verification

- `pvpgn-v3-test:r191` / `pvpgn-v3-test:r191a` still pass identically
  (CMakeLists handle_wserv change is a pure no-op for WITH_BNETD=OFF
  because that whole subdir is skipped).

## Status

- **R192 partial**: Finding 1 fixed; Findings 2 + 3 documented and
  deferred. Defensive CI lane NOT installed yet (it would always
  fail until Finding 3 is repaired).
- **Recommended next round**: R193 = pick + implement a Finding 3
  repair strategy (likely B, two-pass `src/v3`, as the least
  invasive). R194 = re-introduce the `v3-bnetd-on` Dockerfile stage
  on top of R193.
