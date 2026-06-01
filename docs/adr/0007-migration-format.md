# ADR 0007 — Multi-Backend SQL Migration Format

**Status:** Accepted  
**Date:** 2026-06-01  
**Deciders:** PvPGN Core Team  

## Context

Plan 07 consolidates per-backend repository implementations (SQLite, MySQL, PostgreSQL) into a single repository per aggregate, with the storage backend chosen at composition time. This requires a unified migration format that can express dialect-specific SQL while maintaining a single source of truth.

Previously, migrations were scattered across `src/infra/migrations/<backend>/` directories, leading to:
- Schema drift between backends
- Duplicate migration logic
- Difficulty tracking which migrations apply to which backends

## Decision

We adopt a **single migration file per aggregate** with **dialect-specific SQL blocks** marked by `-- dialect:` headers.

### Migration File Format

**Location:** `src/infra/migrations/<aggregate>/Vnnnn__<description>.sql`

**Structure:**
```sql
-- SPDX-License-Identifier: GPL-2.0-or-later
-- Migration: V0001__create_accounts
-- Applies to: all backends (SQLite, MySQL, PostgreSQL)

-- dialect: all
CREATE TABLE accounts (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    locale TEXT NOT NULL DEFAULT 'enUS',
    password_hash TEXT NOT NULL,
    locked INTEGER NOT NULL DEFAULT 0,
    must_change_password INTEGER NOT NULL DEFAULT 0,
    command_groups TEXT NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL DEFAULT 0,
    updated_at INTEGER NOT NULL DEFAULT 0
);

-- dialect: sqlite
CREATE INDEX idx_accounts_name ON accounts(name COLLATE NOCASE);

-- dialect: mysql
CREATE INDEX idx_accounts_name ON accounts(name);
ALTER TABLE accounts CONVERT TO CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- dialect: postgres
CREATE INDEX idx_accounts_name ON accounts(name);
```

### Dialect Markers

- `-- dialect: all` — Applies to all backends (SQLite, MySQL, PostgreSQL)
- `-- dialect: sqlite` — SQLite only
- `-- dialect: mysql` — MySQL only
- `-- dialect: postgres` — PostgreSQL only

### Migration Runner Behavior

The migration runner (`infra/migrations/migration_runner.cpp`):

1. Reads the migration file
2. Splits on `-- dialect:` markers
3. Executes only the blocks matching the active backend
4. Tracks applied migrations in a `schema_migrations` table (backend-specific schema)

### Schema Migrations Table

Each backend maintains its own `schema_migrations` table:

**SQLite:**
```sql
CREATE TABLE schema_migrations (
    version TEXT PRIMARY KEY,
    description TEXT NOT NULL,
    installed_on DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    execution_time_ms INTEGER NOT NULL
);
```

**MySQL:**
```sql
CREATE TABLE schema_migrations (
    version VARCHAR(50) PRIMARY KEY,
    description VARCHAR(255) NOT NULL,
    installed_on TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    execution_time_ms INT NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

**PostgreSQL:**
```sql
CREATE TABLE schema_migrations (
    version VARCHAR(50) PRIMARY KEY,
    description VARCHAR(255) NOT NULL,
    installed_on TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    execution_time_ms INTEGER NOT NULL
);
```

## Rationale

1. **Single source of truth:** One migration file per aggregate eliminates drift
2. **Explicit dialect markers:** Clear which SQL applies where; easy to audit
3. **Backward compatible:** Existing per-backend migrations can be consolidated incrementally
4. **Testable:** Migration runner can be tested against all three backends in CI
5. **Maintainable:** Schema changes require one edit, not three

## Consequences

### Positive

- Schema parity enforced by design
- Easier to spot dialect-specific bugs
- Reduced maintenance burden
- CI can run migration tests against all backends

### Negative

- Migration files are longer (multiple dialect blocks)
- Requires careful testing of dialect-specific SQL
- Migration runner must be robust to parsing errors

## Implementation Notes

1. **Existing migrations:** Consolidate `src/infra/migrations/{sqlite,mysql,postgres}/*` into `src/infra/migrations/<aggregate>/` with dialect markers
2. **New migrations:** Always use the unified format
3. **CI gate:** Fail if a migration file lacks a `-- dialect: all` or backend-specific block
4. **Documentation:** Update `docs/developer/contexts/` with migration examples per bounded context

## References

- Plan 07: Infra Adapter Rehab
- `src/infra/migrations/migration_runner.cpp`
- `src/infra/persistence/repository_factory.cpp`
