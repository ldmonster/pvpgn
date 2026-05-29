# R340 — Coverage CI Artefact

## Checklist
- [x] Read `.github/workflows/` directory (was empty — no existing workflows)
- [x] Created `.github/workflows/v3-coverage.yml` — triggers on push to `main`/`develop` and PRs to `main`; installs `ninja-build lcov libsqlite3-dev`; configures with `cmake --preset v3-coverage`; builds; runs `ctest --preset v3-coverage`; generates lcov report; uploads to Codecov; uploads `coverage.info` as a GitHub Actions artefact
- [x] Created `.github/workflows/v3-sanitizers.yml` — two jobs: `asan-ubsan` (ASan+UBSan via `v3-asan` preset) and `tsan` (TSan via `v3-tsan` preset); both run on `ubuntu-24.04`; triggers on push to `main`/`develop` and PRs to `main`

## Result
Two new GitHub Actions workflows are in place:
- `v3-coverage.yml` runs the coverage build, generates an lcov report, uploads it
  to Codecov (non-blocking), and stores `coverage.info` as a downloadable artefact.
- `v3-sanitizers.yml` runs AddressSanitizer+UBSan and ThreadSanitizer builds in
  parallel jobs, catching memory errors and data races on every push/PR.
Both workflows use the new `v3-*` CMake presets from R339.
