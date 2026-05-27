# R196.b -- WITH_D2CS=ON build CLOSED + `v3-d2cs-on` CI lane added

## Goal

Apply the R194 + R195 playbook to d2cs: get the legacy `d2cs`
executable building under `WITH_D2CS=ON + PVPGN_BUILD_V3=ON`, then
lock it in with a Dockerfile CI lane. R193.audit predicted the same
systemic CMake ordering bug as bnetd; R196.b proves it (and proves
the d2cs surface is far smaller than bnetd's, so the empirical fix
was tiny).

## Findings

### Finding A: CMake ordering bug (R193 class, d2cs side)

Same systemic issue as R193.fix:

```
add_subdirectory(src/v3)   # v3 sees `d2cs_legacy` MISSING
add_subdirectory(src)      # creates `d2cs_legacy` too late
```

Inside `src/v3/CMakeLists.txt:1274` the gate
`if(TARGET d2cs_legacy)` around `integration_legacy_d2cs_linked`
was always FALSE, so the linked target was never created. The
downstream gate in `src/d2cs/CMakeLists.txt:58`
(`if(TARGET integration_legacy_d2cs_linked)`) was also always
FALSE, so `PVPGN_V3_D2CS_INTEGRATION=1` was never defined on
`d2cs_legacy`, so every `#ifdef PVPGN_V3_D2CS_INTEGRATION` block
in d2cs handlers was dead code under any `WITH_D2CS=ON` build.

### Finding B: prefs_v3_shim.h `#ifdef` guard (R194 Finding 9 class)

`src/d2cs/prefs_v3_shim.h:24` used
`#ifdef PVPGN_V3_D2CS_INTEGRATION` to guard
`#include "integration/legacy_d2cs/d2cs_prefs_bridge.hpp"`.
Once Finding A was repaired, the macro was defined on
`d2cs_legacy` -- but the shim's `inline` accessor bodies still
needed the bridge function declarations even when transitively
pulled into TUs without the macro. This produced ~70
"`pvpgn_v3_d2cs_prefs_get_*` not declared in this scope" errors.

## Fixes

### Fix A: lift integration wiring into a late-added CMakeLists

- Removed `if(TARGET d2cs_legacy) ... integration_legacy_d2cs_linked ...`
  block from `src/v3/CMakeLists.txt` (~28 lines, replaced with a
  comment pointing here).
- Removed `if(TARGET integration_legacy_d2cs_linked) ...` wiring block
  from `src/d2cs/CMakeLists.txt` (~8 lines, replaced with a comment).
- Created `src/v3/integration/legacy_d2cs/CMakeLists.txt` containing
  both blocks (now guaranteed to run with both `d2cs_legacy` and the
  v3 tree fully realised).
- Added
  `add_subdirectory(src/v3/integration/legacy_d2cs)` to the top-level
  `CMakeLists.txt` immediately after the existing
  `add_subdirectory(src/v3/integration/legacy_bnetd)`.

### Fix B: prefs_v3_shim.h `__has_include` gate

- `src/d2cs/prefs_v3_shim.h:24`:
  `#ifdef PVPGN_V3_D2CS_INTEGRATION` ->
  `#if __has_include("integration/legacy_d2cs/d2cs_prefs_bridge.hpp")`.
  Same one-line fix as R194 applied to `src/bnetd/prefs_v3_shim.h`.

### Fix C: `v3-d2cs-on` Dockerfile stage

- Appended new stage to `Dockerfile.v3` after `v3-bnetd-on` (+38 lines).
- Inherits `v3-base`. Configures with
  `-D PVPGN_BUILD_V3=ON -D PVPGN_BUILD_LEGACY=ON -D WITH_BNETD=OFF
   -D WITH_D2CS=ON -D WITH_D2DBS=OFF -D PVPGN_V3_WARNINGS_AS_ERRORS=OFF
   -D CMAKE_BUILD_TYPE=Release`.
- Builds the `d2cs` target only.
- Asserts binary at `build/v3-d2cs-on/src/d2cs/d2cs` exists.
- `CMD` runs `d2cs --help` for startup smoke.
- NOT promoted to default target.
- Updated `Usage:` header.

## Verification

```
docker build -f Dockerfile.v3 --target v3-d2cs-on -t pvpgn-v3-d2cs-on:r196b .
...
[100%] Built target d2cs_legacy
[100%] Built target d2cs
v3-d2cs-on: built d2cs successfully at build/v3-d2cs-on/src/d2cs/d2cs
naming to docker.io/library/pvpgn-v3-d2cs-on:r196b done
```

Regression checks:
- `v3-bnetd-on` re-built (R195 lane): still green
  (`[100%] Built target bnetd`). The CMake top-level edit didn't
  disturb the bnetd path -- the new late `add_subdirectory` is a
  no-op when `d2cs_legacy` doesn't exist (WITH_D2CS=OFF case).
- `v3-build` lane (default v3-only): rebuilt clean, all v3 test
  targets compile.

## Why this took only 2 fixes (compare R194: 10+)

- d2cs has a **much smaller** strangler surface than bnetd. The
  `integration_legacy_d2cs_linked` lib has 1 source file
  (`send_packet_bridge_link.cpp`) versus bnetd's 22.
- No d2cs-side equivalent of the R165 mass header deletion that
  killed `bnetd/prefs.h`, `bnetd/handle_init.h`, `bnetd/handle_wserv.h`
  and friends -- so no dangling-include cascade in d2cs.
- No d2cs-side equivalent of the `LegacyBridge::init()`
  application-lib extraction -- d2cs doesn't have an asio event-loop
  bridge.
- `prefs_v3_shim.h` shim is structurally identical to the bnetd
  one, so the R194 Finding 9 fix transferred verbatim.

## Files Changed

| File | Change |
|------|--------|
| `CMakeLists.txt` | +5 lines: late `add_subdirectory(src/v3/integration/legacy_d2cs)` |
| `src/v3/CMakeLists.txt` | -29 lines (block removed) / +8 lines (comment) |
| `src/d2cs/CMakeLists.txt` | -8 lines (block removed) / +13 lines (comment) |
| `src/d2cs/prefs_v3_shim.h` | -3 lines / +8 lines (`__has_include` + comment) |
| `src/v3/integration/legacy_d2cs/CMakeLists.txt` | NEW, +75 lines |
| `Dockerfile.v3` | +38 lines (`v3-d2cs-on` stage) + 1 line (Usage:) |

Total: 6 files, +118 / -40.

## What this protects against

- Same as R195 (bnetd-on), but for d2cs.
- R196.b's own restructure: any future revert that puts the
  `if(TARGET d2cs_legacy)` block back into the v3 mid-subdir will
  fail this stage (linked target won't be created).
- Any new `#ifdef PVPGN_V3_D2CS_INTEGRATION` strangler block in
  d2cs handlers that fails to compile under the integration build.

## Out of scope / deferred

- d2dbs lane (R196.c candidate -- same playbook, different target;
  `integration_legacy_d2dbs_linked` does not exist yet, only the
  dispatcher-only scaffold `integration_legacy_d2dbs`).
- Runtime / functional smoke beyond `d2cs --help`.
- GitHub Actions / appveyor.yml wiring (same deferral as R195).
- Strict warnings on legacy d2cs tree (same as bnetd: permanent
  deferral).

## Status

- Build verified: `pvpgn-v3-d2cs-on:r196b` image green.
- Regression: `pvpgn-v3-bnetd-on:r196b` green, `pvpgn-v3-build-check:r196b`
  green.
- All three docs updated (`plans/r196b-checklist.md` (this file),
  `plans/progress-master.md`, `changelog.md`).
