# TOML Schema Versioning

## Overview

PvPGN v3 uses a `schema_version` field at the top of every TOML configuration
file (`bnetd.toml`, `d2cs.toml`, `d2dbs.toml`) to detect configuration
incompatibilities at startup rather than silently misinterpreting settings.

## The `schema_version` Field

Every TOML config file must begin with:

```toml
schema_version = 3
```

This must be a **top-level** assignment — it must appear **before** any
`[section]` header.  The loader scans for it during startup and rejects the
file if:

- The field is **absent** → `ConfigError: Missing required top-level field 'schema_version'`
- The value is **not a positive integer** → `ConfigError: schema_version value is not a valid integer`
- The value is **lower than the minimum** supported by this build → `SchemaMismatch: schema_version N is too old`
- The value is **higher than the maximum** supported by this build → `SchemaMismatch: schema_version N is newer than this build supports`

## Version History

| `schema_version` | PvPGN release | Changes |
|-----------------|---------------|---------|
| 1 | 3.0.0 | Initial TOML config format |
| 2 | 3.1.0 | Added `[persistence]` section; `[storage]` path format extended |
| 3 | 3.2.0 | Added `[plugins]` section; `[loader]` strict-mode key; `schema_version` field made mandatory |

## Upgrade Policy

### Additive changes (no version bump required)

- Adding a new **optional** key to an existing section
- Adding a new **optional** section
- Changing a default value (document in `CHANGELOG.md`)

### Minor version bump (schema_version + 1)

- Renaming a key (provide a `[deprecated]` mirror table for one minor release)
- Removing a key that was previously optional
- Changing the semantics of an existing key

### Major version bump (schema_version reset or large increment)

- Removing a required key
- Restructuring sections in a backward-incompatible way

## Escape Hatch: `[loader] strict = false`

When `schema_version >= 3`, the loader rejects unknown top-level keys by
default (typo guard).  Operators who have hand-edited configs with custom keys
can disable this for one release cycle:

```toml
schema_version = 3

[loader]
strict = false   # Disable unknown-key rejection (deprecated; remove before 3.3.0)
```

> **Warning:** `strict = false` is a temporary escape hatch.  It will be
> removed in the next minor release.  Use it only to buy time to clean up
> your config file.

## Validator Implementation

The validator is implemented in
[`src/infra/config/toml_schema_validator.hpp`](../../src/infra/config/include/infra/config/toml_schema_validator.hpp)
and
[`src/infra/config/src/toml_schema_validator.cpp`](../../src/infra/config/src/toml_schema_validator.cpp).

Key constants:

| Constant | Value | Meaning |
|----------|-------|---------|
| `kTomlSchemaMinSupported` | 1 | Oldest schema this build can load |
| `kTomlSchemaMaxSupported` | 3 | Newest schema this build understands |

## Checking Your Config

```sh
bnetd --check-config conf/bnetd.toml
```

A valid config exits with code 0.  An invalid schema version exits with a
non-zero code and prints a human-readable error message.

## Adding a New Config Key (Canonical Path)

1. Add the key to `conf/bnetd.toml.in` under the appropriate section.
2. If the change is additive (new optional key), no `schema_version` bump is needed.
3. If the change is breaking, increment `schema_version` in `conf/bnetd.toml.in`
   and update `kTomlSchemaMaxSupported` in `toml_schema_validator.hpp`.
4. Update this document's version history table.
5. Add a `CHANGELOG.md` entry under `### Changed` or `### Added`.
6. Regenerate `docs/developer/config-reference.md` via `scripts/dev/gen-config-docs.sh`.
