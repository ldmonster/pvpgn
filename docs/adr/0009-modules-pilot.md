# ADR 0009: C++23 Uplift and Modules Pilot

**Date**: 2026-06-02
**Status**: Accepted
**Deciders**: PvPGN Core Team

## Context

Plan 09 (`plans/09-cpp23-uplift.md`) moves the v3 sub-tree from C++20 to C++23
and asks for a *gated* evaluation of C++ named modules, with the pilot result
recorded in this ADR.

Starting state when this ADR was written:

- The v3 tree compiled as C++20 (`PVPGN_V3_CXX_STANDARD=20`).
- There is **no `tl::expected`** in the dependency graph: `core::Result<T,E>`
  is a hand-rolled `std::variant`-based "expected" (see `core/result.hpp`),
  so Plan 09's "drop `tl::expected`" criterion is already satisfied. The type
  can later be re-backed by `std::expected` as a non-breaking internal change.
- There are **no `fmt::print` / `printf` calls** in the v3 layers
  (`core/domain/application/protocol/infra`); logging goes through
  `core/format.hpp` (`LOG_*`) and `core::log`. Remaining `printf`-family calls
  live only in the standalone `tools/*` utilities.

## Decision

1. **Standard bump.** `PVPGN_V3_CXX_STANDARD` is set to **23** in
   `cmake/v3.cmake`; all v3 targets get `cxx_std_23` via
   `target_compile_features`. Toolchain floor: GCC 14 / Clang 18 / MSVC 19.40
   (MinGW matching GCC). Verified building under GCC 13 locally (C++23 mode)
   and GCC 15 in the `Dockerfile.v3` CI image — full tree, `-Werror`, green.

2. **`std::expected`.** Not adopted as a hard cutover in this step. `core::Result`
   stays as the public vocabulary type; migrating its *implementation* to
   `std::expected` is a tracked, non-breaking follow-up now that C++23 is the
   floor. No external `expected` dependency remains to remove.

3. **`std::print`.** Adopted. All ~309 `std::fprintf`/`std::printf` calls in
   `tools/*` were converted to `std::print` / `std::println`; **no
   `printf`-family call remains anywhere in the v3 tree**.
   **Floor note:** `std::print`/`std::println` require **GCC 14+** (libstdc++'s
   `<print>`); GCC 13 supports most of C++23 but not `<print>`. So GCC 13 now
   builds the whole tree *except* the `tools/*` executables — consistent with
   the Plan 09 floor (GCC 14 / Clang 18 / MSVC 19.40). The authoritative CI
   build (`Dockerfile.v3`, GCC 15) builds everything, `-Werror`, green.

4. **Modules pilot — DEFERRED (gated, not adopted).** We do **not** convert
   `src/core/strings/` (or any code) to a named module at this time.
   Rationale, per the plan's "document blockers and stop" guidance:

   - **Matrix cannot be validated here.** Plan 09 requires modules to be green
     on GCC 14, Clang 18, *and* MSVC 19.40 before adoption. This environment can
     only exercise GCC (13 local / 15 Docker); Clang and MSVC module support for
     this code cannot be verified, so the gate cannot be cleared.
   - **Toolchain brittleness.** Named modules still interact poorly with
     `vcpkg`, CMake's scanner across compilers, and **Catch2** test TUs (the
     unit suite is the project's primary safety net). The plan explicitly calls
     this out as a risk and makes pilot failure acceptable.
   - **Low marginal benefit now.** The headers are already small and
     layering-clean; the build-time win from modularising one leaf library does
     not justify the cross-compiler risk before the matrix exists.

   When a CI matrix with all three toolchains is in place (Plan 10 lays the
   groundwork for the sanitiser/coverage matrix), revisit by converting
   `core/strings` behind `-DPVPGN_MODULES=ON` and re-scoring this ADR.

## Consequences

- The v3 tree is C++23. `std::expected`, deducing-this, `std::flat_map`,
  `[[assume]]`, etc. are now available for incremental adoption where they pay
  for themselves (the latter two gated on Plan 13 benchmarks).
- No modules are introduced, so the build graph, IDE tooling, and Catch2 test
  TUs are unchanged; risk stays low.
- Downstream packagers must move to the GCC 14 / Clang 18 / MSVC 19.40 floor;
  announce in `CHANGELOG.md` one minor release ahead per the plan's risk note.

## Status of Plan 09 acceptance criteria

- [~] `cmake --build` passes with C++23 — verified on GCC 13 (local) and
  GCC 15 (Docker `v3-build`, `-Werror`); Clang 18 / MSVC 19.40 / MinGW columns
  pending a CI matrix this environment cannot run.
- [x] `tl::expected` no longer in the dependency graph (never present;
  `core::Result` is self-contained).
- [~] No `printf`/`fprintf`/`fmt::print` outside a `// MIGRATION` block — true
  for the v3 layers; the `tools/*` utilities are a tracked follow-up.
- [x] Modules pilot result documented (this ADR): **deferred/gated**.
