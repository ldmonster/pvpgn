# MySQL / PostgreSQL backend schema reconciliation

Scope: do the MySQL and PostgreSQL backends have a schema matching what the
repos write, or are they broken the way SQLite was before the embedded-migration
reconciliation?

## TL;DR / overall status

The premise of the task ("the v3 repos also support MySQL and PostgreSQL
backends ... the repos are backend-agnostic — same SQL via the driver") does
**not hold in production**. The reality is:

1. **There is NO MySQL/PostgreSQL schema anywhere in the repository.** Zero
   `CREATE TABLE` statements target mysql/pg. The only DDL that exists is the
   SQLite-specific embedded migrations (`all_migrations.cpp`) and the matching
   `.sql` files, and that DDL is *only* executed by the SQLite UoW factory.
   The mysql/pg UoW factories never run migrations and never create any tables.

2. **The backend-agnostic `Sql*Repository` classes never run on mysql/pg.**
   They are driven by `IDbDriver`, whose only implementation is
   `sqlite_driver.hpp`. There is no MySQL or PostgreSQL `IDbDriver`. The
   `RepositoryFactory` that wires `Sql*Repository` over a driver is **never
   constructed in production code** (grep shows it is only used in tests).

3. **What the mysql/pg backends actually run (production path):** the UoW
   factories build a *native* account repository
   (`MySQLAccountRepository` / `PostgreSQLAccountRepository`) and **in-memory
   stubs** for everything else (clans, ladder, ip_bans, account_bans,
   friend_lists, realms, channels, games, teams). So on mysql/pg:
   - `accounts` is the only table touched, and it has **no schema to create it**
     — the operator must hand-create it, and even then there are column
     mismatches (see findings below).
   - All other domains (bans, clans, ladder, friends, realms, channels) are
     **non-durable in-memory** — they silently lose data on restart and never
     touch the database at all.

So the answer to "do mysql/pg get a schema?": **No.** And the answer to "are the
mysql/pg backends usable?": **No, not as a durable multi-table store.** Accounts
will fail at first query/insert because no `accounts` table exists; everything
else is in-memory.

Note this is a *different* failure mode than the SQLite-pre-fix bug. SQLite at
least had a schema (just mismatched columns). mysql/pg have **no schema at all**
plus an architecture that bypasses the agnostic repos entirely.

---

## Evidence map

| Concern | Location |
|---|---|
| Production backend selection | `src/app/bnetd/src/main.cpp:277-337` |
| MySQL UoW factory (no migrations) | `src/infra/mysql/src/unit_of_work_factory.cpp:71-76` |
| Postgres UoW factory (no migrations) | `src/infra/postgres/src/unit_of_work_factory.cpp:72-77` |
| MySQL UoW wiring (account=native, rest=in-memory) | `src/infra/mysql/src/unit_of_work.cpp:37-48` |
| Postgres UoW wiring (account=native, rest=in-memory) | `src/infra/postgres/src/unit_of_work.cpp:38-49` |
| SQLite UoW factory DOES run migrations | `src/infra/sqlite/src/unit_of_work_factory.cpp:17-41` |
| Only migrations are SQLite-specific | `src/infra/migrations/src/all_migrations.cpp` |
| Only `IDbDriver` impl is SQLite | `src/infra/persistence/sql_builder/sqlite_driver.hpp` (no mysql/pg equivalent) |
| `RepositoryFactory` only used in tests | `src/infra/persistence/repository_factory.cpp` (no production constructor call) |
| No mysql/pg DDL anywhere | repo-wide `grep "CREATE TABLE"` -> only migrations files |

---

## Findings

### F1 — No MySQL/PostgreSQL schema exists; the backends create no tables
- **Severity:** Critical
- **Classification:** NOT-IMPLEMENTED (presents as BUG to an operator who selects `backend="mysql"`/`"postgres"`)
- **v3 ref:** `src/infra/mysql/src/unit_of_work_factory.cpp:71-76`,
  `src/infra/postgres/src/unit_of_work_factory.cpp:72-77`
- **Divergence:** The SQLite factory runs `MigrationRunner.migrate_to_latest(...)`
  in its constructor (`unit_of_work_factory.cpp:38-40`), creating all 11 tables.
  The mysql/pg factories just open a connection and return a UoW — no
  `ensure_migration_table`, no `migrate_to_latest`, no DDL. There is no
  `sql_dbcreator` equivalent, no `.sql` schema for mysql/pg, and the embedded
  migrations are never invoked on these backends. Result: the first query to
  `accounts` fails with "table doesn't exist" unless the operator manually
  created tables out-of-band.
- **Proposed fix:** Either (a) implement mysql/pg `IDbDriver`s and route the
  agnostic `Sql*Repository` set through `RepositoryFactory` for these backends
  (then the embedded migrations must be made portable — see F4), or (b) ship a
  mysql/pg-specific schema (a `sql_dbcreator`-style `.sql` per dialect) and run
  it from the mysql/pg UoW factories. Until then, document mysql/pg as
  unsupported.

### F2 — Only `accounts` is persisted on mysql/pg; all other repositories are in-memory stubs (silent data loss)
- **Severity:** Critical
- **Classification:** NOT-IMPLEMENTED (data-loss footgun)
- **v3 ref:** `src/infra/mysql/src/unit_of_work.cpp:40-48`,
  `src/infra/postgres/src/unit_of_work.cpp:41-49`
- **Divergence:** The UoW constructors wire `channels_`, `games_`, `clans_`,
  `ladder_`, `ip_bans_`, `account_bans_`, `friend_lists_`, `realms_`, `teams_`
  to `inmemory::*` repositories. The header comments admit "Non-account
  repositories fall back to in-memory implementations". So clans, ladder, bans,
  friends, realms persist nowhere and vanish on restart even though the operator
  asked for a durable SQL backend. This is the opposite of what the
  task assumed ("the repos write tables: accounts, account_attributes,
  account_bans, ... ladder, realms, channels") — on mysql/pg none of those
  secondary tables are ever written.
- **Proposed fix:** Implement native mysql/pg repositories for the remaining
  domains (or the driver-based path in F1), and only then claim mysql/pg
  durability. Until then the UoW should at least log a loud warning that
  non-account data is non-durable, mirroring the sqlite-not-compiled fallback
  warning in `main.cpp:331`.

### F3 — `accounts` column mismatch between native mysql/pg repos and the (nonexistent) schema
- **Severity:** High (manifests if an operator hand-creates `accounts` from the SQLite DDL)
- **Classification:** BUG
- **v3 ref:** MySQL `save` at `src/infra/mysql/src/account_repository.cpp:202-217`;
  Postgres `save` at `src/infra/postgres/src/account_repository.cpp:209-248`
- **Divergence:** Both native repos `SELECT ... created_at, updated_at` and the
  MySQL `INSERT` writes `created_at, updated_at` (hard-coded `0`). The Postgres
  INSERT omits `created_at` entirely and relies on `updated_at = NOW()` in the
  `ON CONFLICT` branch, i.e. it assumes `created_at`/`updated_at` columns exist
  with server-side defaults — which no shipped schema provides. The Postgres
  new-account branch (`account_repository.cpp:210-227`) inserts without `id` and
  depends on an auto-increment/`BIGSERIAL` `id` column; the SQLite DDL declares
  `id INTEGER PRIMARY KEY` with no serial. So even if an operator copies the
  SQLite `accounts` DDL, Postgres inserts of new accounts (id==0) will fail or
  insert id 0. The header comment claims "BIGSERIAL for auto-increment" but no
  schema declares it.
- **Proposed fix:** Provide a mysql schema with
  `created_at BIGINT NOT NULL DEFAULT 0, updated_at BIGINT NOT NULL DEFAULT 0`
  and a pg schema with `id BIGSERIAL PRIMARY KEY, created_at TIMESTAMPTZ,
  updated_at TIMESTAMPTZ`. Reconcile the repos' INSERT column lists against it.

### F4 — Embedded migration SQL is SQLite-only; it cannot be reused as the mysql/pg schema
- **Severity:** High (blocks the "make the agnostic path portable" remedy)
- **Classification:** BUG (portability) / by-design for SQLite
- **v3 ref:** `src/infra/migrations/src/all_migrations.cpp`
- **Divergence:** The migration DDL uses several SQLite-only constructs that
  will not run on mysql/pg:
  - `COLLATE NOCASE` on columns and indexes (lines 20, 33, 82, 126, 139) —
    not valid in MySQL or PostgreSQL (pg has no `NOCASE`; mysql uses
    `_ci` collations / `CHARACTER SET`).
  - `id INTEGER PRIMARY KEY` as an auto-rowid (lines 19, 77, 119) — on mysql
    needs `AUTO_INCREMENT`, on pg needs `SERIAL`/`BIGSERIAL`/`GENERATED`.
  - `INTEGER PRIMARY KEY AUTOINCREMENT` for `channels` (line 138) —
    `AUTOINCREMENT` is a SQLite keyword; invalid on both others.
  - `created_at ... DEFAULT (strftime('%s','now'))` (line 143) — `strftime`
    is SQLite-only.
  - `_schema_migrations.version INTEGER PRIMARY KEY` plus the runner's
    `CREATE TABLE IF NOT EXISTS` (`migration_runner.cpp:18-25`) is portable, but
    the data migrations are not.
- **Proposed fix:** If the driver-based agnostic path is chosen for mysql/pg,
  the migrations need per-dialect variants (or an abstraction that emits the
  right keywords per backend). Otherwise keep migrations SQLite-only and ship
  separate hand-written mysql/pg schema files.

### F5 — Agnostic `Sql*Repository` SQL is SQLite-specific (would break if ever routed to mysql/pg)
- **Severity:** Medium (latent — only bites if F1's driver path is implemented)
- **Classification:** BUG (portability)
- **v3 ref (the SQLite-only SQL in the agnostic repos):**
  - `INSERT OR REPLACE` — SQLite-only. mysql wants `REPLACE INTO` or
    `INSERT ... ON DUPLICATE KEY UPDATE`; pg wants `INSERT ... ON CONFLICT`.
    Occurrences:
    - `src/infra/persistence/account_ban_repository.cpp:83`
    - `src/infra/persistence/channel_repository.cpp:99`
    - `src/infra/persistence/game_repository.cpp:103`
    - `src/infra/persistence/ladder_repository.cpp:67`
    - `src/infra/persistence/realm_repository.cpp:92`
    - (also `src/infra/persistence/account_repository.cpp` — comments at
      `:167-177` confirm this was historically `INSERT OR REPLACE`; verify the
      current upsert form there is portable.)
  - `COLLATE NOCASE` in WHERE clauses — not portable:
    - `src/infra/persistence/channel_repository.cpp:47`
      (`WHERE name = ? COLLATE NOCASE`)
    - `src/infra/persistence/realm_repository.cpp:67`
      (`WHERE name = ? COLLATE NOCASE`)
    - `src/infra/persistence/account_repository.cpp:131` (`COLLATE NOCASE`)
  Note the native mysql/pg account repos already sidestep this by using
  `LOWER(name) = LOWER(...)` instead of `COLLATE NOCASE`
  (`mysql/.../account_repository.cpp:162`,
  `postgres/.../account_repository.cpp:163`), confirming the agnostic SQL is not
  intended to run on those backends.
- **Proposed fix:** If the agnostic repos are ever pointed at a mysql/pg driver,
  the upserts and case-insensitive predicates must be dialect-abstracted (e.g.
  driver-provided `upsert()` / `ci_eq()` helpers, or per-dialect SQL builders).

---

## Bottom line for the caller

- The recent SQLite migration↔repo reconciliation does **not** carry over to
  mysql/pg, because mysql/pg never use those migrations or those agnostic repos.
- mysql/pg are **not usable as durable backends today**: no schema is created
  (F1), and only `accounts` would even be attempted while everything else is
  in-memory (F2). Even `accounts` has column/auto-increment mismatches against
  the only DDL that exists (F3).
- The agnostic repos and the embedded migrations are both SQLite-locked (F4,
  F5), so the "same SQL via the driver" story is aspirational, not implemented.
- Recommended framing: classify mysql/pg as NOT-IMPLEMENTED rather than "broken
  schema". The fix is substantial (native repos or mysql/pg drivers + portable
  schema), not a one-line DDL reconciliation like the SQLite fix was.
