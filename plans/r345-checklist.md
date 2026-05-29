# R345 — CI Matrix Expansion

## Status: COMPLETE

## What was done

### New file: `.github/workflows/v3-ci.yml`
- **Triggers**: push/PR to `main` and `v3/**` branches
- **Strategy**: `fail-fast: false` — all matrix legs run even if one fails
- **5 matrix legs**:

| name | os | preset | cc | cxx | extras |
|------|----|--------|----|-----|--------|
| `linux-gcc-debug` | ubuntu-24.04 | `v3-dev` | gcc-14 | g++-14 | — |
| `linux-clang-asan-ub` | ubuntu-24.04 | `v3-asan` | clang-18 | clang++-18 | — |
| `linux-clang-tsan` | ubuntu-24.04 | `v3-tsan` | clang-18 | clang++-18 | — |
| `linux-coverage` | ubuntu-24.04 | `v3-coverage` | gcc-14 | g++-14 | lcov + Codecov |
| `integration-mysql-pg` | ubuntu-24.04 | `v3-dev` | gcc-14 | g++-14 | MySQL 8.0 + PostgreSQL 16 services |

- **Base dependencies** (all jobs): `cmake ninja-build libsqlite3-dev libssl-dev` + compiler
- **Integration dependencies** (integration job only): `libmysqlclient-dev libpq-dev`
- **Services** (integration job only):
  - MySQL 8.0: health check via `mysqladmin ping`, port 3306
  - PostgreSQL 16: health check via `pg_isready`, port 5432
  - Services use conditional image expressions (`matrix.integration && 'image' || ''`) so non-integration jobs don't start them
- **Configure**: `cmake --preset ${{ matrix.preset }}` with `CC`/`CXX` env vars
- **Build**: `cmake --build --preset ${{ matrix.preset }}`
- **Unit tests**: `ctest --preset ${{ matrix.preset }} --output-on-failure`
- **Integration tests** (integration job only):
  - Sets `PVPGN_TEST_MYSQL_DSN` and `PVPGN_TEST_PG_DSN` env vars
  - Runs `ctest --preset v3-dev --label-regex integration`
- **Coverage** (linux-coverage job only):
  - `lcov --capture` → filter `/usr/*`, `*/tests/*`, `*/third_party/*` → `lcov --list`
  - Uploads to Codecov via `codecov/codecov-action@v4`
  - Uploads `coverage.info` as 30-day artifact

## Design decisions
- `fail-fast: false` ensures all matrix legs report independently — a sanitizer failure doesn't hide a coverage failure
- Services use conditional image expressions rather than separate jobs to keep the matrix compact
- `--label-regex integration` on the integration ctest run ensures only integration tests run (not unit tests again)
- `lcov` step is conditional on `matrix.preset == 'v3-coverage'` to avoid running on other legs
- Compiler installation is conditional: clang-18 for ASAN/TSAN legs, gcc-14 for others
