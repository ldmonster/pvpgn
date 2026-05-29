# R338 — Integration Test Skeleton + docker-compose

## Checklist
- [x] Created `tests/integration/CMakeLists.txt` — defines `test_integration` executable; links `pvpgn_infra_sqlite`, `pvpgn_infra_mysql`, `pvpgn_infra_postgresql`, `pvpgn_infra_migrations`, `pvpgn_infra_file`, Catch2; gated behind `PVPGN_V3_INTEGRATION_TESTS=ON`
- [x] Created `tests/integration/main.cpp` — Catch2 session entry point with ANSI colour output
- [x] Created `tests/integration/account_repository_integration_test.cpp` — integration tests for SQLiteAccountRepository (on-disk), FileAccountRepository (temp dir), MySQL (skipped if `PVPGN_TEST_MYSQL_DSN` unset), PostgreSQL (skipped if `PVPGN_TEST_PG_DSN` unset)
- [x] Created `tests/integration/README.md` — explains how to run integration tests locally and in CI
- [x] Created `docker-compose.integration.yml` at project root — MySQL 8.0 + PostgreSQL 16 with health checks
- [x] Updated `tests/CMakeLists.txt` — added `add_subdirectory(integration)`

## Result
The `tests/integration/` directory provides a complete integration test skeleton.
SQLite and file-based tests always run against real on-disk storage.
MySQL and PostgreSQL tests are skipped unless the corresponding DSN environment
variables (`PVPGN_TEST_MYSQL_DSN`, `PVPGN_TEST_PG_DSN`) are set.
The `docker-compose.integration.yml` file spins up MySQL 8.0 and PostgreSQL 16
with health checks for CI use.
