# 09 — C++23 Uplift

## What

Move the v3 tree from C++20 to C++23. Adopt `std::expected`,
`std::print` / `std::println`, `std::flat_map` where it fits,
`std::mdspan` where it fits, `[[assume]]`, deducing-this. Evaluate
modules behind a feature flag.

## Why

- The codebase already uses `tl::expected` (or similar) in places.
  Replacing with `std::expected` removes a vendor dep.
- `std::print` removes the last `printf` family calls.
- Deducing-this collapses a class of CRTP boilerplate in
  `domain/<ctx>/` aggregates.

## Prerequisites

- Toolchain floor lifted to GCC 14 / Clang 18 / MSVC 19.40.
- vcpkg baseline pinned and committed.
- Plans 02, 03, 05 done so legacy code is gone and won't drag the
  toolchain matrix back down.

## Concrete steps

1. **CI matrix update.** Add GCC 14, Clang 18, MSVC 19.40 columns;
   drop GCC ≤ 12, Clang ≤ 16, MSVC < 19.40.
2. **CMake.** Bump `CMAKE_CXX_STANDARD` to 23 in `cmake/v3.cmake`.
   Add `target_compile_features(... cxx_std_23)` on `pvpgn_v3_base`.
3. **`tl::expected` → `std::expected`** by codemod (sed + compile).
   Delete the vendored or vcpkg dependency.
4. **`fmt::print` → `std::print`** in `core/log/` and tools. Keep
   `fmt` as a fallback only where formatting custom types isn't
   migrated yet; track removal as a follow-up.
5. **Modules pilot.** Convert `src/core/strings/` to a named module
   behind `-DPVPGN_MODULES=ON`. If green on all three compilers,
   expand; otherwise document blockers and stop. Do **not** convert
   public headers until all three toolchains agree.
6. **`std::flat_map` / `std::flat_set`** where the access pattern is
   small-N read-heavy (lookup tables in `protocol/bnet/codec/`).
7. **`[[assume]]`** in tight inner loops only when a benchmark proves
   gain (plan 13).
8. **Deducing-this** to remove CRTP in domain aggregates where it
   exists.

## Acceptance criteria

- [ ] `cmake --build` passes with C++23 on GCC 14, Clang 18, MSVC
      19.40, MinGW-w64 (matching GCC).
- [ ] `tl::expected` no longer in the dependency graph.
- [ ] No `printf`/`fprintf`/`fmt::print` call outside an explicitly
      annotated `// MIGRATION` block.
- [ ] Modules pilot result documented in
      `docs/adr/0009-modules-pilot.md`.

## Risks

- Toolchain matrix shrinkage may break downstream packagers. Announce
  the floor bump in `CHANGELOG.md` one minor release before enforcing.
- Modules + Catch2 + vcpkg interactions are brittle. Pilot is gated;
  failure is acceptable and recorded in the ADR.

## Out of scope

- Coroutines (covered in plan 06).
- C++26 features.
