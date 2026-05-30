# 09 — Build system modernization

## What

Reduce CMake surface area, lean on presets + vcpkg, drop the parallel legacy build path once plan 06 completes.

## Concrete steps

### CMake hygiene

1. **One CMakeLists per target.** Today some folders (e.g. `src/common`) build a kitchen-sink library. Split into one `add_library` per cohesive group: `pvpgn_net`, `pvpgn_hash`, `pvpgn_text`, …
2. **Target-only include dirs.** No `include_directories(...)` at script scope; use `target_include_directories(... PUBLIC|PRIVATE ...)`.
3. **Target-only link.** No global `link_libraries`. Every `add_library` declares its `target_link_libraries` with `PUBLIC` / `PRIVATE` / `INTERFACE`.
4. **Generated headers** (`config.h`, `version.h`) live in `${CMAKE_BINARY_DIR}/include/` and are exposed via a single `pvpgn_generated_headers` INTERFACE library.
5. **Remove `CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE`** once vendored copies of `pugixml` etc. are gone.
6. **Drop legacy preset** from `CMakePresets.json` after plan 06.

### vcpkg

7. Pin every dependency in `vcpkg.json` with `version>=` and `baseline`. No transitive surprises.
8. Add `vcpkg-configuration.json` with overlays for any patched port.
9. CI uses the binary cache (`actions/cache` keyed on `vcpkg.json` + baseline).

### Compile-time discipline

10. `set(CMAKE_CXX_STANDARD 20)` is in. Add `set(CMAKE_CXX_STANDARD_REQUIRED ON)` and `set(CMAKE_CXX_EXTENSIONS OFF)` (already done).
11. Treat warnings as errors per target (MSVC `/W4 /WX`, gcc/clang `-Wall -Wextra -Wpedantic -Werror`). Vendored dirs opt out via `SYSTEM` includes + per-target `-w`.
12. Enable `/utf-8` on MSVC, `-finput-charset=utf-8` on gcc, to avoid the cp-1252 em-dash trap noted in user memory.
13. Enable `LINK_TIME_OPTIMIZATION` on the `v3-release` preset; off elsewhere for fast incremental builds.
14. Enable `INTERPROCEDURAL_OPTIMIZATION_RELEASE` per target via `CheckIPOSupported`.

### Presets simplification

After plan 06:

```jsonc
// CMakePresets.json
{
  "configurePresets": [
    "v3-dev",         // Debug, all warnings, tests on
    "v3-release",     // Release, LTO, tests on
    "v3-asan",        // Debug + AddressSanitizer
    "v3-tsan",        // Debug + ThreadSanitizer
    "v3-coverage",    // Debug + gcov
    "v3-fuzz"         // Clang + libFuzzer
  ],
  "buildPresets":  [ matching ],
  "testPresets":   [ matching ]
}
```

Delete `legacy`, `linked`, `dev-release` presets.

### Build dir layout

15. Build artefacts: `build/<preset>/`. Today there are stale `build/legacy/` and `build/linked/`; document in `docs/building.md` that they are obsolete after plan 06.

## Acceptance criteria

- [ ] `cmake --preset v3-dev && cmake --build --preset v3-dev` is a green-field workflow with no extra flags.
- [ ] `cmake --list-presets` shows ≤ 6 presets, all `v3-*`.
- [ ] No `include_directories` call at script (non-target) scope anywhere in `src/`.
- [ ] CI cold-build under 10 minutes; warm-build under 2 minutes (with vcpkg cache).
- [ ] MSVC `/WX` clean across the whole tree.

## Risks

- `/WX` flips on warnings that were previously tolerated. Plan a one-PR sweep per warning class (`-Wshadow`, `-Wconversion`, …).

## Out of scope

- C++ modules (`import std;`). Toolchain support is still uneven; revisit in a year.
