# R213 -- Header Self-Contained Test Generator

**Date:** 2026-05-27
**Round:** 213
**Phase:** 1 (Foundations)
**Plan ref:** [plans/01-modern-cpp-baseline.md](../plans/01-modern-cpp-baseline.md) §3
**Status:** GREEN ✓

## Objective

Add a reusable CMake helper that, given a list of public headers,
auto-generates one .cpp per header that just `#include`s it and
compiles them all into a single Catch2 test executable. If any
header is not self-contained (forgets a transitive include, depends
on order), the compile fails -- which is the whole point.

Back-fill the helper for `src/v3/core/include/core/*.hpp` as the
first consumer; other libraries will be added in later rounds.

## Changes applied

- [x] `cmake/v3.cmake`: new `pvpgn_v3_add_header_selfcheck(name
      INCLUDE_ROOT ... HEADERS ... DEPS ...)` function. Generates
      one TU per header in
      `${CMAKE_CURRENT_BINARY_DIR}/<name>_gen/` plus a Catch2 stub.
- [x] `tests/unit/core/CMakeLists.txt`: invoke the helper for the
      16 public `core/*.hpp` headers (15 existing + `core/cxx.hpp`
      added in R210).
- [x] `Dockerfile.v3`: added `test_core_headers_selfcontained` to
      the `--target` list in the v3-build stage and to the test
      RUN chain in the v3-test stage.

## Build verification

```powershell
docker build -f Dockerfile.v3 --target v3-test -t pvpgn-v3-test:r213 .
```

All 16 generated TUs (`include_core_<hdr>_hpp.cpp.o`) compiled,
linked, and the test executed in the v3-test RUN chain. Image
tagged `pvpgn-v3-test:r213`. Conclusion: every `core/*.hpp` header
is self-contained under C++20 / Alpine Clang+GCC.

## Files touched

### Modified
- `cmake/v3.cmake` -- new helper function.
- `tests/unit/core/CMakeLists.txt` -- selfcheck invocation.
- `Dockerfile.v3` -- new target in v3-build + v3-test stages.

### Created
- `plans/r213-checklist.md` -- this file.
- (generated at build time: `tests/unit/core/test_core_headers_
  selfcontained_gen/include_core_*_hpp.cpp` + `_selfcheck_main.cpp`).

## Follow-ups

- Back-fill selfcheck targets for `src/v3/domain/**/include/**/*.hpp`,
  `src/v3/application/**/include/**/*.hpp`, `src/v3/protocol/**/
  include/**/*.hpp`, `src/v3/infra/**/include/**/*.hpp` in subsequent
  rounds. Track as R213a/b/c per layer (or fold into the R215 sweep).
