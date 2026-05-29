# R352 Checklist — Change default of PVPGN_BUILD_LEGACY to OFF

## Changes
- [x] Changed `option(PVPGN_BUILD_LEGACY ...)` default from `ON` to `OFF` in CMakeLists.txt
- [x] Added deprecation `message(WARNING ...)` when `PVPGN_BUILD_LEGACY=ON` in CMakeLists.txt
- [x] Wrapped legacy subdirectories (`common`, `compat`, `win32`, `bnetd`, `d2cs`, `d2dbs`) in `if(PVPGN_BUILD_LEGACY)` guard in `src/CMakeLists.txt`
- [x] Updated `README.md` with v3-first build instructions and legacy opt-in note

## Result
New builds default to v3-only. Legacy build is opt-in via `-DPVPGN_BUILD_LEGACY=ON`.
A deprecation warning is emitted at configure time when legacy is enabled.
