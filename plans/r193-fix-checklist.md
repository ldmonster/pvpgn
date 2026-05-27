# R193.fix Checklist — Strategy B (two-pass `src/v3`)

Started: 2026-05-27. Goal: repair R192 Finding 3 (CMake ordering
bug) by splitting off the bnetd_legacy-dependent v3 wiring into a
separate `src/v3/integration/legacy_bnetd/CMakeLists.txt` that is
added AFTER `add_subdirectory(src)` in the top-level configure.

## Plan recap (Strategy B)

The original top-level order was:
```
add_subdirectory(src/v3)   # tries to create integration_legacy_bnetd_linked
                           # under if(TARGET bnetd_legacy ...) but bnetd_legacy
                           # doesn't exist yet -> target never created
add_subdirectory(src)      # creates bnetd_legacy; references the never-created
                           # target under if(TARGET ...) which is FALSE
                           # -> PVPGN_V3_BNETD_INTEGRATION never set -> dead code
```

New order under Strategy B:
```
add_subdirectory(src/v3)                              # most of v3 (NO linked target)
add_subdirectory(src)                                 # creates bnetd_legacy
add_subdirectory(src/v3/integration/legacy_bnetd)     # NEW: creates linked
                                                      # target + does all bnetd
                                                      # wiring + adds server_v3_hook
```

## Changes landed

- [x] **NEW** `src/v3/integration/legacy_bnetd/CMakeLists.txt`
      consolidates THREE previously-scattered (and previously-dead)
      blocks:
  1. `integration_legacy_bnetd_linked` target creation (was
     `src/v3/CMakeLists.txt:1352-1410`). Source paths rewritten
     from `integration/legacy_bnetd/src/foo.cpp` to `src/foo.cpp`
     (relative to the new subdir).
  2. `bnetd` / `bnetd_legacy` strangler wiring including
     `target_compile_definitions(bnetd_legacy PRIVATE
     PVPGN_V3_BNETD_INTEGRATION=1)` and the long list of v3 include
     paths (was `src/bnetd/CMakeLists.txt:121-156`).
  3. `server_v3_hook.cpp` injection onto `bnetd_legacy` (was
     `src/v3/app/bnetd/CMakeLists.txt:100-117`).
- [x] `src/v3/CMakeLists.txt`: removed the
      `integration_legacy_bnetd_linked` creation block (60+ lines),
      replaced with a 7-line "MOVED TO ..." comment.
- [x] `src/v3/app/bnetd/CMakeLists.txt`: removed the
      `server_v3_hook` injection block, replaced with a comment.
- [x] `src/bnetd/CMakeLists.txt`: removed the
      `if(TARGET integration_legacy_bnetd_linked) ... endif()` block
      (~35 lines), replaced with a comment.
- [x] Top-level `CMakeLists.txt`: added a second
      `if(PVPGN_BUILD_V3) add_subdirectory(...) endif()` block
      AFTER `add_subdirectory(src)`, pulling in the new subdir.

## Verification

### Configure-time (R193_VERIFY probe)

Same Alpine container as R193.audit. Configure with
`-DPVPGN_BUILD_V3=ON -DPVPGN_BUILD_LEGACY=ON -DWITH_BNETD=ON
-DWITH_D2CS=OFF -DWITH_D2DBS=OFF -DCMAKE_BUILD_TYPE=Release`.

**Before R193.fix** (recorded in R193.audit):
```
R193_PROBE: integration_legacy_bnetd_linked MISSING
R193_PROBE: bnetd_legacy COMPILE_DEFINITIONS=_r193_defs-NOTFOUND
```

**After R193.fix** (recorded this round):
```
R193_VERIFY: integration_legacy_bnetd_linked EXISTS
R193_VERIFY: bnetd_legacy COMPILE_DEFINITIONS=PVPGN_V3_BNETD_INTEGRATION=1
R193_VERIFY: bnetd COMPILE_DEFINITIONS=PVPGN_V3_BNETD_INTEGRATION=1
```

Both probes were removed from `CMakeLists.txt` after their runs.

### v3-test regression

`docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r193 .`
completed; final stage `naming to docker.io/library/pvpgn-v3-test:r193
done` succeeded. All Catch2 test executables that grep matched
output `All tests passed (... assertions in ... test cases)` -- no
FAILED, no `error:`. Image size 1.46GB, consistent with R191a.

WITH_BNETD=ON build itself is NOT yet exercised by Dockerfile.v3 --
that's R194's job. R193.fix only proves the CMake graph is now
consistent.

## Not in scope this round

- Finding 2 (`prefs.cpp` underscore name mismatch) -- still
  deferred. With R193.fix landed, an attempted WITH_BNETD=ON build
  will now reach prefs.cpp and surface this dormant bug.
- R194: re-introduce the `v3-bnetd-on` Dockerfile stage exercising
  the legacy executable. Will need Finding 2 fix first plus any
  other latent build errors that surface now that the graph is no
  longer silently dead.
- Audit whether earlier rounds (R168, R171, R172, R191.a) introduced
  any behavioural bugs that were masked by the dead code. Since the
  strangler hooks couldn't run, any incorrect handler logic has been
  hidden. Worth a careful read once R194 is green.

## Status

- **R193.fix COMPLETE under Strategy B.** Configure-time probe
  confirms the systemic bug is repaired. v3-test pipeline unchanged
  (no regression).
- Recommended next: R193.fix.2 = fix Finding 2, OR R194 = author the
  v3-bnetd-on Dockerfile stage and iteratively triage whatever else
  surfaces.
