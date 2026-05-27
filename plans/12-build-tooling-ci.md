# 12 — Build, Tooling & CI

**Goal:** Reproducible local + CI builds across Linux/macOS/Windows
with linting, formatting, static analysis and layering checks
gating every PR.

## 1. Build presets (`CMakePresets.json`)

Add/standardise:

| Preset | Toolchain | Type | Notable |
|--------|-----------|------|---------|
| `v3-dev` | host default | Debug | warnings as errors off, tests on |
| `v3-release` | host default | Release | LTO on |
| `v3-asan` | clang | Debug | -fsanitize=address,undefined |
| `v3-tsan` | clang | Debug | -fsanitize=thread |
| `v3-msvc-release` | MSVC | Release | static vcpkg triplet |
| `v3-coverage` | gcc | Debug | --coverage |
| `v3-fuzz` | clang | Debug | -fsanitize=fuzzer,address |
| `legacy-only` | host | Release | `PVPGN_BUILD_V3=OFF`, `PVPGN_BUILD_LEGACY=ON` |

`PVPGN_BUILD_LEGACY` defaults to OFF after R295 (see
`14-legacy-retirement.md`).

## 2. Compiler flags

Per-target, not global:

```cmake
function(pvpgn_v3_target_warnings tgt)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE
      /W4 /permissive- /Zc:__cplusplus /Zc:preprocessor
      /wd4244 /wd4267)  # narrowing — review and remove
  else()
    target_compile_options(${tgt} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wshadow -Wconversion -Wnon-virtual-dtor
      -Wold-style-cast -Wcast-align -Wnull-dereference
      -Wdouble-promotion -Wformat=2
      -Wno-unused-parameter)  # too noisy until we sweep
  endif()
endfunction()

function(pvpgn_v3_target_werror tgt)
  if(MSVC) target_compile_options(${tgt} PRIVATE /WX)
  else()   target_compile_options(${tgt} PRIVATE -Werror) endif()
endfunction()
```

Apply `pvpgn_v3_target_warnings` to every v3 target. Apply
`pvpgn_v3_target_werror` to `core`, `domain`, `application`,
`protocol` (the layers that should be lint-clean today). Add infra
once it's swept.

## 3. clang-format

- `.clang-format` already exists (verify). Style: based on `Google`,
  4-space indent, ColumnLimit 100, `BinPackArguments: false`.
- Pre-commit hook (script `scripts/dev/format.sh`) runs
  `clang-format -i` on all changed `src/v3/**` files.
- CI runs `scripts/dev/format-check.sh` and fails on diff.

## 4. clang-tidy

- `.clang-tidy` at repo root with these check families enabled:
  `bugprone-*`, `cert-*`, `concurrency-*`, `cppcoreguidelines-*`
  (selective: skip `pro-bounds-pointer-arithmetic` until protocol
  layer is span-only), `misc-*`, `modernize-*`, `performance-*`,
  `portability-*`, `readability-*` (skip `magic-numbers`),
  `hicpp-*`.
- Disabled noise: `modernize-use-trailing-return-type`,
  `readability-named-parameter`, `cppcoreguidelines-avoid-magic-numbers`.
- CI runs clang-tidy on `src/v3/**` per PR (incremental on changed
  files only — fast). Nightly: full repo.
- `// NOLINT(check-name)` is allowed with mandatory inline reason.

## 5. IWYU

Optional, advisory. Nightly job posts diff suggestions to a PR
comment. Not a gate.

## 6. Layering checks

`scripts/ci/check_layering.sh` (specified in
`05-ports-and-adapters.md` §5). Required PR check.

## 7. CI matrix (GitHub Actions, AppVeyor already used)

```
linux-gcc-release    (PR + main)
linux-gcc-debug      (PR + main)
linux-clang-asan-ub  (PR + main)
linux-clang-tsan     (main only — slow)
linux-coverage       (main, artefact only)
macos-clang-release  (PR + main)
windows-msvc-release (PR + main — existing)
android-?            (not in scope)
fuzz-nightly         (cron, main)
integration-mysql-pg (PR + main; uses docker compose)
e2e-compose          (PR + main; uses scripts/dev/v3-compose-smoke.sh)
```

All matrix jobs must be green before merge. `tsan`, `coverage`,
`fuzz` are advisory (don't block) initially; flip to required after
two clean weeks.

## 8. Dependency hygiene

- vcpkg manifest is the source of truth on Windows. On Linux/macOS
  prefer system packages; FetchContent fallback for `fmt` and
  `nlohmann_json` (see `/memories/repo/pvpgn-deps.md`).
- Pin versions: `fmt 10.x`, `nlohmann_json 3.11.3`, `catch2 3.5.4`,
  `tomlplusplus 3.4.x`, `sol2 3.x` (when scripting moves to it; see
  `13-plugin-and-scripting.md`).
- Renovate / dependabot bot proposes bumps; humans accept.
- `LICENSES.third_party.md` generated from vcpkg + manual table.

## 9. Reproducibility

- Pin a Docker base image hash in `Dockerfile.v3` (`alpine:3.20` →
  pinned digest). Same for any CI runner image override.
- Build is deterministic given the source + vcpkg lock + base image.

## 10. Concrete tasks

- [ ] R292: introduce `pvpgn_v3_target_warnings` /
      `pvpgn_v3_target_werror` helpers, apply to current clean
      layers.
- [ ] R293: `.clang-tidy` + nightly + per-PR incremental tidy.
- [ ] R294: layering CI check (depends on `05-ports-and-adapters.md`).
- [ ] R295: sanitizer matrix (depends on `11-testing-strategy.md`).
