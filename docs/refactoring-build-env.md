# Building the legacy daemons locally

The legacy bnetd / d2cs / d2dbs daemons can now be built from a
clean checkout. This document captures the bootstrap pieces that
were restored to the tree as part of Batch 32a, and the one
remaining external dependency.

## Quick start

```powershell
# Configure -- legacy only (no v3 sub-tree, no Boost needed)
cmake -S . -B build/legacy `
    -G "Visual Studio 18 2026" -A x64 `
    -DPVPGN_BUILD_LEGACY=ON `
    -DPVPGN_BUILD_V3=OFF `
    -DWITH_BNETD=ON `
    -DWITH_LUA=OFF `
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

# Build everything
cmake --build build/legacy --config Release
```

The first configure will fetch zlib 1.3.1 from
`https://github.com/madler/zlib` (about 1 MB) and build it
in-tree under `build/legacy/_deps/zlib_bootstrap-*`. Subsequent
configures reuse the cached archive.

## Mixed legacy + v3 build

```powershell
cmake -S . -B build/mixed `
    -G "Visual Studio 18 2026" -A x64 `
    -DPVPGN_BUILD_LEGACY=ON `
    -DPVPGN_BUILD_V3=ON `
    -DPVPGN_V3_WITH_BOOST=OFF `
    -DWITH_BNETD=ON `
    -DWITH_LUA=OFF `
    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"

cmake --build build/mixed --config Release
ctest --test-dir build/mixed -C Release
```

With `PVPGN_V3_WITH_BOOST=OFF` the v3 sub-tree builds the pure
domain / application / infra-inmemory targets and all of their
unit tests, but skips Boost-dependent targets (notably
`infra_net` and `integration_legacy_bnetd_linked`).

## Linked variant (`integration_legacy_bnetd_linked`)

The strangler-fig bridge that actually links against
`bnetd_legacy` lives behind `PVPGN_V3_WITH_BOOST=ON`. It requires:

- **Boost 1.75 or newer** with at least `system` (and `fiber`,
  `context` if `PVPGN_V3_WITH_FIBER=ON`).

Boost is not currently bootstrapped automatically; install it via
your platform package manager or vcpkg:

```powershell
# Option A -- vcpkg (manifest mode, picks up vcpkg.json automatically)
vcpkg install --triplet x64-windows-static
cmake -S . -B build/full ... -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake

# Option B -- system Boost
$env:BOOST_ROOT = "C:\local\boost_1_85_0"
cmake -S . -B build/full ...
```

A `vcpkg.json` manifest is included at the repository root for
Option A.

## What was restored

`cmake/Modules/DefineInstallationPaths.cmake`
  Defines `*_INSTALL_DIR` variables from `GNUInstallDirs`. The
  legacy install rules read these directly.

`cmake/Modules/CheckMkdirArgs.cmake`
  Provides the `check_mkdir_args(VAR)` macro that
  `ConfigureChecks.cmake` invokes. On Windows it hard-codes the
  single-argument form (`_mkdir`) to avoid a header-only compile
  test misreporting POSIX availability under MSVC.

`cmake/Modules/cmake_uninstall.cmake.in`
`cmake/Modules/cmake_purge.cmake.in`
  Standard install-manifest-driven uninstall / purge templates.

`ConfigureChecks.cmake`
  `find_package(ZLIB REQUIRED)` -> `find_package(ZLIB QUIET)` with
  a `FetchContent` fallback to upstream zlib 1.3.1.

`CMakeLists.txt`
  The top-level project now declares `C CXX` (was `CXX` only) so
  CheckMkdirArgs' `check_c_source_compiles` works. When
  `PVPGN_BUILD_V3=ON` and `PVPGN_BUILD_LEGACY=ON` the global
  `-DUNICODE / -D_UNICODE` flags are stripped before
  `add_subdirectory(src/v3)` so v3 executables keep their narrow
  `main()` entry point.

`vcpkg.json`
  Manifest declaring `zlib` (and optional features for `lua`,
  `sqlite3`, `libmysql`) for users who prefer vcpkg over the
  in-tree FetchContent fallback.
