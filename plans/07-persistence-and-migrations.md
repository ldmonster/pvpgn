# 07 — Persistence & Schema Migrations

**Goal:** Replace the legacy `storage*.cpp` / `sql_*.cpp` /
`file_plain.cpp` web with a clean repository layer and a versioned
schema-migration system that works identically for SQLite, MySQL,
PostgreSQL, and the legacy plaintext file backend.

## 1. Layers

```
application/<bc>/ports/<Aggregate>Repository.h   <-- port (interface)
infra/persistence/sqlite/<Aggregate>RepositorySqlite.{h,cpp}
infra/persistence/mysql/<Aggregate>RepositoryMysql.{h,cpp}
infra/persistence/postgres/<Aggregate>RepositoryPg.{h,cpp}
infra/persistence/file/<Aggregate>RepositoryFile.{h,cpp}     <-- plaintext compat
infra/persistence/inmemory/<Aggregate>RepositoryInMemory.h   <-- test fake
infra/migrations/                                            <-- versioned schema
```

## 2. Repository contract

```cpp
class AccountRepository {
public:
  virtual ~AccountRepository() = default;

  [[nodiscard]] virtual core::Result<std::optional<domain::identity::Account>,
                                     core::StatusCode>
      find_by_username(const domain::identity::Username&) = 0;

  [[nodiscard]] virtual core::Result<domain::identity::AccountId,
                                     core::StatusCode>
      create(const domain::identity::NewAccount&) = 0;

  [[nodiscard]] virtual core::Result<void, core::StatusCode>
      update(const domain::identity::Account&) = 0;  // optimistic-concurrency
                                                     // by version field

  [[nodiscard]] virtual core::Result<void, core::StatusCode>
      remove(domain::identity::AccountId) = 0;

  // Transactional batch — runs F under a single unit of work; rolls back
  // if F returns an error Result.
  template<class F>
  core::Result<void, core::StatusCode> transactional(F&& f) {
      return do_transactional([&](auto& uow) { return f(uow); });
  }

protected:
  virtual core::Result<void, core::StatusCode>
      do_transactional(
        std::function<core::Result<void, core::StatusCode>(UnitOfWork&)>) = 0;
};
```

- All methods are `[[nodiscard]]` and return `Result`. No exceptions.
- Aggregate roundtrips are by value; the repository hands out copies.
- Concurrent edits resolved by optimistic version field (`version int
  NOT NULL` column / equivalent). Update with wrong version ⇒
  `StatusCode::Conflict`.

## 3. Schema migrations

A new tool `pvpgn-migrate` (binary in `src/v3/tools/pvpgn-migrate/`)
plus a library `infra/migrations/`.

Migrations are SQL files (per dialect) under
`infra/migrations/sql/{sqlite,mysql,postgres}/`:

```
0001_initial.sql
0002_add_clan_invites.sql
0003_add_ladder_season.sql
...
```

A `schema_version` table tracks the highest applied version. The
bnetd boot sequence calls `migrations::apply(*db, expected_version)`
and aborts startup with a clear log if the DB is older than the
binary expects. **No** silent on-the-fly schema upgrades — they hide
data loss.

For the plaintext file backend, "migration" means a one-shot
read-old/write-new tool; the binary refuses to run on a mismatched
plaintext version.

## 4. Connection pooling & threading

- Each adapter owns a small connection pool (`infra/persistence/pool.h`,
  generic). Default size = `min(8, hardware_concurrency)`.
- Prepared statements cached per-connection.
- The unit-of-work captures one connection from the pool for its
  lifetime.

## 5. Migrating legacy data

R222 (see `02-finish-strangler-fig.md`) replaces legacy storage by:

1. Implement the new repository on top of the same SQL dialect.
2. Add a feature flag `PVPGN_USE_V3_PERSISTENCE` (config TOML).
   Default OFF in pre-cutover release, ON afterwards.
3. Shadow-write: when both are enabled, write to both, read from
   legacy, compare-on-read. Log mismatches as ERROR.
4. After a stable window (1 release cycle), flip read-side to v3,
   keep shadow-write to legacy for one more cycle.
5. Remove legacy storage code and the shadow flag.

## 6. File backend (`storage_file.cpp` successor)

The legacy plaintext backend stays as a first-class adapter — it's
the default for home servers and has zero dependencies. The new
file backend:

- One file per account (existing convention).
- Format upgrade: TOML-per-account instead of bespoke
  `attribute=value` pairs (matches the rest of the project's TOML
  convention). Provide a one-shot converter in `pvpgn-migrate
  --from-plain --to-toml-file`.
- Atomic writes: `write tmp + fsync + rename`. Never partial writes.
- File locking via `flock` / `LockFileEx`. Adapter owns a
  per-directory mutex.

## 7. Concrete tasks

- [ ] R262: define `UnitOfWork`, the migrations library, the
      `pvpgn-migrate` tool skeleton, the in-memory account
      repository fake.
- [ ] R263: sqlite `AccountRepositorySqlite` + migration `0001`.
- [ ] R264: parity-test it against legacy `storage_sql.cpp` SQLite mode.
- [ ] R265: mysql + postgres adapters.
- [ ] R266: file (TOML) adapter + migrator.
- [ ] R267: shadow-write infra and feature flag.
- [ ] R268…: remaining repositories (channel, game, clan, …) one per
      round.
- [ ] R269 (final): delete legacy `storage*.cpp` and `sql_*.cpp`.

## 8. Risks

- **Data loss on migrate** — shadow-write phase is mandatory.
- **MySQL/Postgres dialect drift** — keep migrations as separate files
  per dialect; do not auto-translate.
- **Plaintext file format change** — converter tool is mandatory and
  must round-trip; ship a release with both formats supported before
  the cutover release.
