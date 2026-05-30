# ADR 0005: Use Catch2 v3.5.4 as Test Framework

**Date**: 2026-05-30  
**Status**: Accepted  
**Deciders**: PvPGN Core Team

## Context

The legacy PvPGN codebase had no automated test suite.  As the v3
refactoring introduced domain logic and application use cases that could be
unit-tested, the team needed to choose a C++ test framework.

Requirements:

- **Header-only or easy to vendor**: the project already uses vcpkg; the
  framework should be available there.
- **No external test runner required**: tests should be self-contained
  executables that CTest can discover and run.
- **Good CMake integration**: `add_executable` + `target_link_libraries`
  should be sufficient; no custom build system required.
- **Expressive assertions**: `REQUIRE`, `CHECK`, `REQUIRE_THROWS_AS`, etc.
  should read naturally.
- **BDD-style sections**: `SECTION` / `GIVEN` / `WHEN` / `THEN` macros for
  describing complex scenarios.
- **Fuzz testing support**: the framework should not conflict with
  libFuzzer-based fuzz targets.
- **Active maintenance**: the framework must be actively maintained and
  compatible with C++17.

Frameworks evaluated:

| Framework | Notes |
|-----------|-------|
| **Google Test (gtest)** | Widely used; requires separate gmock for mocking; heavier dependency; opinionated about test naming |
| **Catch2 v2** | Header-only; good CMake support; but v2 is in maintenance mode |
| **Catch2 v3** | Modular (no longer single-header); actively maintained; C++14+ |
| **doctest** | Fastest compile times; subset of Catch2 API; smaller community |
| **Boost.Test** | Part of Boost; heavy dependency; not suitable for a lean build |

## Decision

Use **Catch2 v3.5.4** as the sole unit and integration test framework for
all new tests in `tests/`.

- Catch2 is declared as a vcpkg dependency in `vcpkg.json`.
- CMake finds it via `find_package(Catch2 3 REQUIRED)`.
- All test executables link `Catch2::Catch2WithMain` (for tests with a
  `main()` provided by Catch2) or `Catch2::Catch2` (for tests that provide
  their own `main()`).
- `include(Catch)` + `catch_discover_tests()` registers each `TEST_CASE`
  as a separate CTest test, enabling parallel execution and per-test
  reporting.
- Fuzz targets in `tests/fuzz/` use libFuzzer directly and do not link
  Catch2.
- The `tests/unit/` directory mirrors the `src/` layout:
  `tests/unit/domain/<ctx>/` for domain tests,
  `tests/unit/application/<ctx>/` for use-case tests.

The minimum coverage target is **80%** for `src/domain/` and
`src/application/` (excluding strangler bridges in `src/integration/`).
Coverage is measured with `gcov`/`llvm-cov` and reported via the
`v3-coverage` CMake preset.

## Consequences

**Positive:**
- Catch2 v3 is actively maintained and tracks the C++ standard.
- `catch_discover_tests()` gives per-`TEST_CASE` granularity in CTest
  output, making it easy to identify failing tests in CI.
- The BDD macros (`GIVEN`/`WHEN`/`THEN`) make complex protocol test
  scenarios readable.
- vcpkg manages the version; upgrading is a one-line change in `vcpkg.json`.
- No conflict with libFuzzer fuzz targets.

**Negative / Trade-offs:**
- Catch2 v3 is no longer a single header; it must be installed (via vcpkg)
  before tests can be compiled.  This adds a vcpkg bootstrap step to CI.
- Compile times for large test files are higher than with doctest.
- The 80% coverage requirement is aspirational until the full test suite is
  written; it is currently blocked by the volume of untested legacy code.

**Blocked items (as of 2026-05-30):**
- 80% coverage requirement: blocked pending full test suite implementation
  across all domain and application modules.
- CI timing metrics (cold-build < 10 min, warm-build < 2 min): blocked
  pending CI infrastructure with vcpkg cache.
