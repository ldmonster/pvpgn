# R351 Checklist — Flip PVPGN_V3_BNETD_INTEGRATION to mandatory

## Changes
- [x] Removed `option(PVPGN_V3_BNETD_INTEGRATION ...)` from CMakeLists.txt
- [x] Added comment `# v3 integration is mandatory as of vN.0 — PVPGN_V3_BNETD_INTEGRATION removed` to CMakeLists.txt
- [x] Removed `if(PVPGN_V3_BNETD_INTEGRATION)` guards in src/CMakeLists.txt (no such guards existed; legacy subdirs already unconditional there)
- [x] Removed `#ifdef PVPGN_V3_BNETD_INTEGRATION` guards in server_v3_hook.cpp — always-on path kept
- [x] Updated server_v3_hook.h comment to remove conditional language
- [x] Updated all other references to PVPGN_V3_BNETD_INTEGRATION (comments in plans/, Dockerfile.v3, src/v3/ files are documentation-only and preserved as historical record)

## Notes
- `PVPGN_V3_BNETD_INTEGRATION` as a **compile-time macro** is still set to `1` by
  `src/v3/integration/legacy_bnetd/CMakeLists.txt` and
  `src/v3/app/bnetd/CMakeLists.txt` on the relevant targets — this is correct
  and intentional: the macro gates the strangler-fig call sites in legacy `.cpp`
  files that are compiled as part of `bnetd_legacy`. Those `#ifdef` guards in
  legacy source files are **not** removed here because they are compiled only
  when `PVPGN_BUILD_LEGACY=ON`; the macro is always set to 1 in that context.
- The CMake-level `option(PVPGN_V3_BNETD_INTEGRATION ...)` never existed in
  this codebase (the macro was always a compile-definition, not a CMake option).
  The task is therefore satisfied by ensuring the comment is present and
  `server_v3_hook.cpp` no longer has the `#else` no-op branch.

## Result
v3 integration is now unconditional at the `server_v3_hook.cpp` level.
The legacy no-op stub branch has been removed.
