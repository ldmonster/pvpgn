# 07 — Infra Adapter Rehab

## What

Collapse the parallel `infra/sqlite/`, `infra/mysql/`,
`infra/postgres/` adapters into one repository implementation per
aggregate, with the storage backend chosen at composition time via a
single SQL-builder layer.

## Why

- Today each aggregate has three near-identical `*_repository.cpp`
  files. Schema changes require three edits; bugs are fixed in one and
  forgotten in the others.
- The SQL dialects differ in trivial ways (parameter markers,
  `RETURNING`, `INSERT OR REPLACE` vs `ON CONFLICT`). A 200-line SQL
  builder handles them all.

## Prerequisites

- Plan 05 done (ports live in `domain/<ctx>/ports/`).

## Concrete steps

1. **SQL builder.** Add `src/infra/persistence/sql_builder/` with:
   - `dialect.hpp` enum + free functions for placeholder and
     upsert syntax.
   - `query.hpp` typed query builder (no ORM).
   - Driver shims `sqlite_driver.cpp`, `mysql_driver.cpp`,
     `postgres_driver.cpp` exposing one interface: `execute(query,
     params) -> result_set`.
2. **One repository per aggregate.** Move each
   `infra/<backend>/<aggregate>_repository.cpp` into
   `infra/persistence/<aggregate>_repository.cpp` parameterized over
   the driver.
3. **Composition.** `infra/persistence/repository_factory.cpp` builds
   the chosen driver from `[storage].backend` in `bnetd.toml`.
4. **Migrations.** Consolidate `infra/migrations/<backend>/*` into
   `infra/migrations/<aggregate>/Vnnnn__name.sql` with `-- dialect:
   sqlite|mysql|postgres` headers; the migration runner picks the
   matching block. ADR `0007-migration-format.md`.
5. **Delete** `src/infra/sqlite/`, `src/infra/mysql/`,
   `src/infra/postgres/` per-aggregate repositories (keep the driver
   shims under `infra/persistence/drivers/`).
6. **Repository tests.** Each repository test runs against all three
   drivers via parameterized fixtures (sqlite in-memory, mysql /
   postgres via testcontainers in CI).

## Acceptance criteria

- [ ] Exactly one `*_repository.cpp` per aggregate.
- [ ] `infra/{sqlite,mysql,postgres}/` contain only driver
      adapters, no aggregate code.
- [ ] CI runs the repository test matrix against all three backends.
- [ ] Switching `[storage].backend` requires no recompilation.

## Risks

- Schema parity drift between backends today may hide silent data
  loss. Migrations PR must include a `schema-diff` test that fails on
  divergence.
- Testcontainers in CI add cold-start time. Cache images aggressively.

## Out of scope

- Adding a new backend (e.g. clickhouse, redis).
- Rewriting the on-disk character-save format (plan 04 covers d2dbs
  parity separately).
