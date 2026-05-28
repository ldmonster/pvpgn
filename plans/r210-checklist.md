# R210 — C++20 Baseline Bump

**Date:** 2026-05-27
**Round:** 210
**Phase:** 1 (Foundations)
**Plan ref:** [plans/01-modern-cpp-baseline.md](../plans/01-modern-cpp-baseline.md) §1
**Status:** GREEN ✓

## Objective

Raise the global `CMAKE_CXX_STANDARD` from 11 to 20 so the whole tree
-- legacy + v3 -- compiles under modern C++. The v3 tree already
opts into C++20 via `PVPGN_V3_CXX_STANDARD` in
[cmake/v3.cmake](../cmake/v3.cmake), so only the legacy tree changed
language version in practice.

## Changes applied

- [x] `CMakeLists.txt`: `set(CMAKE_CXX_STANDARD 11)` -> `20`; added
      a comment block referencing plan 01.
- [x] Added `src/v3/core/include/core/cxx.hpp` static_assert guard
      (`__cplusplus < 202002L` -> hard error).
- [x] Docker `v3-test` build GREEN (image
      `pvpgn-v3-test:r210` tagged).
- [x] Full ctest suite passes: hundreds of test cases across `core`,
      `domain`, `application`, `infra`, `protocol`. Sample tail:
      "All tests passed (1225 assertions in 213 test cases)" plus
      ~30 other suites all green.

## Build verification

Local native configure currently fails on this host because CMake 4.3
removed `FindBoost` and no vcpkg/Boost is installed locally.
**Unrelated to R210**: `PVPGN_V3_WITH_BOOST=OFF` also fails because
`infra_metrics` unconditionally links `Boost::system` (pre-existing
issue; recorded as follow-up). The canonical CI path is Docker,
which is GREEN.

Verification command:

```powershell
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r210 .
```

## Files touched

### Modified
- `CMakeLists.txt` -- `CMAKE_CXX_STANDARD` bumped to 20.

### Created
- `src/v3/core/include/core/cxx.hpp` -- language-standard guard.
- `plans/r210-checklist.md` -- this file.

## Follow-ups

- Pre-existing: `PVPGN_V3_WITH_BOOST=OFF` build is broken because
  `infra_metrics` always links `Boost::system`. Track separately;
  not in scope for R210.
- Local-dev path needs vcpkg bootstrap docs or a Boost-less build
  mode that actually works. Add to plan 12 (build/CI) follow-ups.
