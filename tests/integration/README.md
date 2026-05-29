# Integration Tests

This directory contains integration tests that exercise real storage backends
(SQLite on-disk, MySQL 8, PostgreSQL 16).

## Prerequisites

- CMake ≥ 3.21 with Ninja
- Docker + Docker Compose (for MySQL / PostgreSQL tests)
- `libsqlite3-dev` (for SQLite tests)

## Running locally

### SQLite and File tests (no Docker required)

```bash
cmake --preset dev-debug -DPVPGN_V3_INTEGRATION_TESTS=ON
cmake --build --preset dev-debug
ctest --test-dir build/dev-debug -L integration --output-on-failure
```

### MySQL + PostgreSQL tests

1. Start the compose stack:

```bash
docker compose -f docker-compose.integration.yml up -d
# Wait for health checks to pass (≈ 30 s)
docker compose -f docker-compose.integration.yml ps
```

2. Export the DSN environment variables:

```bash
export PVPGN_TEST_MYSQL_DSN="mysql://pvpgn:pvpgn_test@localhost:3306/pvpgn_test"
export PVPGN_TEST_PG_DSN="postgresql://pvpgn:pvpgn_test@localhost:5432/pvpgn_test"
```

3. Run the integration tests:

```bash
ctest --test-dir build/dev-debug -L integration --output-on-failure
```

4. Tear down the stack when done:

```bash
docker compose -f docker-compose.integration.yml down -v
```

## Running in CI

The GitHub Actions workflow `.github/workflows/v3-integration.yml` (planned)
will:

1. Start the compose stack using `docker compose -f docker-compose.integration.yml up -d`.
2. Wait for health checks.
3. Set `PVPGN_TEST_MYSQL_DSN` and `PVPGN_TEST_PG_DSN` from CI secrets.
4. Build with `-DPVPGN_V3_INTEGRATION_TESTS=ON`.
5. Run `ctest -L integration`.

## Environment variables

| Variable | Description |
|---|---|
| `PVPGN_TEST_MYSQL_DSN` | MySQL DSN. If unset, MySQL tests are skipped. |
| `PVPGN_TEST_PG_DSN` | PostgreSQL DSN. If unset, PostgreSQL tests are skipped. |

## Adding new integration tests

1. Create a new `*_integration_test.cpp` file in this directory.
2. Add it to the `_INTEGRATION_SOURCES` list in `CMakeLists.txt`.
3. Guard MySQL/PostgreSQL sections with `if (dsn.empty()) { SKIP(...); }`.
