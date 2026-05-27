# R193.audit Checklist — empirically prove Finding 3 (CMake ordering bug)

Started: 2026-05-27. Goal: before committing to a multi-strategy
repair, run a cheap cmake-configure probe to **prove** that
`integration_legacy_bnetd_linked` is genuinely never created and
`PVPGN_V3_BNETD_INTEGRATION` is genuinely never defined under
`WITH_BNETD=ON + PVPGN_BUILD_V3=ON`. R192 derived this by reading
CMake source; R193.audit re-verifies by actual configure.

## Method

1. Added a temporary probe block to the top-level `CMakeLists.txt`
   immediately after `add_subdirectory(src)` (i.e. after BOTH the v3
   tree and the legacy `src` tree have been processed):
   - `if(TARGET integration_legacy_bnetd_linked) ... message(...)`
   - `get_target_property(_r193_defs bnetd_legacy COMPILE_DEFINITIONS)`
     and print it.
2. Ran cmake configure in an Alpine container (matches
   `Dockerfile.v3`'s toolchain): `apk add build-base clang cmake
   make git boost-dev openssl-dev zlib-dev curl-dev`, then `cmake -S
   /src -B /tmp/probe -DPVPGN_BUILD_V3=ON -DPVPGN_BUILD_LEGACY=ON
   -DWITH_BNETD=ON -DWITH_D2CS=OFF -DWITH_D2DBS=OFF
   -DCMAKE_BUILD_TYPE=Release`.
3. Filtered for `R193_PROBE` lines and CMake errors.
4. Reverted the probe.

## Result (verbatim console output)

```
-- R193_PROBE: integration_legacy_bnetd_linked MISSING
-- R193_PROBE: bnetd_legacy COMPILE_DEFINITIONS=_r193_defs-NOTFOUND
```

No CMake errors during configure -- so the bug is silent (configure
"succeeds" then build fails ~80 times in `prefs_v3_shim.h`).

## Conclusion

- [x] **Finding 3 confirmed**. Under `WITH_BNETD=ON +
      PVPGN_BUILD_V3=ON`:
  - `integration_legacy_bnetd_linked` does NOT exist after both v3
    and `src` subdirs are processed -> R192's analysis at
    `src/v3/CMakeLists.txt:1352 if(TARGET bnetd_legacy AND
    PVPGN_V3_WITH_BOOST)` being FALSE at v3-time is correct.
  - `bnetd_legacy` exists but has **no `COMPILE_DEFINITIONS` at
    all** -> `target_compile_definitions(bnetd_legacy PRIVATE
    PVPGN_V3_BNETD_INTEGRATION=1)` at
    `src/bnetd/CMakeLists.txt:144` never fires -> R192's "dead-code"
    claim about all install_*_handler wiring is correct.
- [x] Finding 1 fix (handle_wserv removal) is sufficient for
      configure to complete cleanly -- no other dangling references.

## Status

- **R193.audit DONE**. Bug is real, reproducible, silent at
  configure time. Repair must follow.
- **R193.fix PENDING** -- pick from strategies A/B/C/D in
  `plans/r192-checklist.md`. Recommended: **strategy B (two-pass
  `src/v3`)** because:
  - It preserves the `src/v3` ownership of v3 build wiring (vs A).
  - It is statically analysable (vs C).
  - It does not invert any existing `if(TARGET v3_*)` check in
    `src/bnetd/CMakeLists.txt` (vs D, which would silently break
    all of them).

## Probe code (for reference)

Inserted between `add_subdirectory(src)` and `if(WITH_LUA)` at
`CMakeLists.txt:212`:

```cmake
# >>> R193.audit probe (remove before commit) <<<
if(TARGET integration_legacy_bnetd_linked)
    message(STATUS "R193_PROBE: integration_legacy_bnetd_linked EXISTS")
else()
    message(STATUS "R193_PROBE: integration_legacy_bnetd_linked MISSING")
endif()
if(TARGET bnetd_legacy)
    get_target_property(_r193_defs bnetd_legacy COMPILE_DEFINITIONS)
    message(STATUS "R193_PROBE: bnetd_legacy COMPILE_DEFINITIONS=${_r193_defs}")
else()
    message(STATUS "R193_PROBE: bnetd_legacy MISSING")
endif()
# <<< R193.audit probe (remove before commit) >>>
```

Reverted after the run; `CMakeLists.txt` is back to its
post-R191.a state.
