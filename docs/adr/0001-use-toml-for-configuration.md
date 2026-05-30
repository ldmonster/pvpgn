# ADR 0001: Use TOML for Configuration (schema_version = 3)

**Date**: 2026-05-30  
**Status**: Accepted  
**Deciders**: PvPGN Core Team

## Context

PvPGN historically used a bespoke line-oriented `.conf` format (one key per
line, `key = value`, no sections, no nesting).  As the server grew, this
format became a maintenance burden:

- No standard parser existed; the in-house parser was ~800 LOC of fragile
  string manipulation.
- Nested configuration (e.g., per-realm settings, per-game version checks)
  required multiple separate files (`bnetd.conf`, `realm.conf`,
  `versioncheck.conf`, …), making deployment error-prone.
- No schema validation: typos in key names were silently ignored.
- No support for arrays or inline tables, forcing comma-separated strings
  for list values.
- The format was not human-friendly for operators unfamiliar with PvPGN
  internals.

By PvPGN 3.0 the codebase had accumulated 15+ `.conf` files that operators
had to manage simultaneously.

## Decision

Adopt **TOML** (Tom's Obvious Minimal Language) as the single configuration
format for all PvPGN v3 daemons, with a mandatory `schema_version = 3` key
at the top of every config file.

- `conf/bnetd.toml.in`, `conf/d2cs.toml.in`, and `conf/d2dbs.toml.in`
  replace all legacy `.conf` files.
- 12 previously separate `.conf.in` files are inlined as TOML tables.
- `sql_DB_layout.conf.in` is deleted (superseded by migration tooling).
- A `pvpgn-migrate config` tool converts legacy `.conf` trees to TOML.
- `bnetd --check-config` validates the TOML file at startup and exits
  non-zero on schema errors.
- The `schema_version` key allows forward-compatible migrations: the server
  rejects configs with an unsupported version and prints a migration hint.

The TOML library used is the header-only **toml++** (v3.x), vendored under
`vendor/tomlplusplus/`.

## Consequences

**Positive:**
- Single config file per daemon; operators no longer juggle 15 files.
- Standard TOML parsers exist for every language, simplifying tooling.
- Schema validation at startup catches typos before they cause runtime errors.
- Nested tables and arrays are first-class, eliminating comma-separated hacks.
- `schema_version` enables safe, documented migrations between PvPGN releases.

**Negative / Trade-offs:**
- Operators must migrate existing `.conf` files; `pvpgn-migrate config`
  automates this but requires a one-time manual step.
- The legacy build path has been removed; all builds now use TOML.
- TOML is slightly more verbose than the old flat format for simple scalar
  settings.

**Status (as of 2026-05-30):**
- `bnetd --check-config` runtime validation requires a running binary (CI scaffolding created).
- `pvpgn-migrate config` tool implemented; round-trip functional test added.
