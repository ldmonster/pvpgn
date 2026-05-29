# R277 — GitHub Actions Fuzz Smoke Workflows

## Status: COMPLETE

## Files Created
- `.github/workflows/fuzz-smoke.yml` — 60s PR smoke on `src/v3/protocol/**` and `tests/fuzz/**` changes
- `.github/workflows/fuzz-nightly.yml` — 30min nightly with crash artifact upload (90-day retention)

## Notes
- `.github/` directory was created fresh — no prior GitHub Actions configuration existed in the repo
- No `.travis.yml` found in the repository; `appveyor.yml` exists but targets Windows/MSVC only
- Binary output path adjusted to `build-fuzz/fuzz/` (not `build-fuzz/tests/fuzz/`) because
  `tests/fuzz/CMakeLists.txt` sets `RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/fuzz"`
- Sanitizer flags use `-fsanitize=fuzzer,address` only (no `undefined`) to match what
  `tests/fuzz/CMakeLists.txt` links via `target_link_libraries` and `target_compile_options`
- `PVPGN_ENABLE_FUZZING=ON` requires `CMAKE_CXX_COMPILER_ID MATCHES "Clang"` — workflows
  use `clang-17` / `clang++-17` to satisfy both conditions
- Fuzz smoke workflow triggers only on PRs touching `src/v3/protocol/**` or `tests/fuzz/**`
- Nightly workflow runs at 02:00 UTC daily and supports `workflow_dispatch` for manual runs
- Crash artifacts: smoke uploads on `failure()` only; nightly uploads `always()` with
  `if-no-files-found: ignore` to avoid failing clean runs
