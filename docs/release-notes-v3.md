# PvPGN 3.0.0 Release Notes

## Overview

PvPGN 3.0.0 is the culmination of a full DDD + Hexagonal Architecture refactoring
(Phases A–N, rounds R210–R354). The v3 sub-tree is now the primary codebase.
Legacy sources (`src/bnetd/`, `src/d2cs/`, `src/d2dbs/`) remain available via
`-DPVPGN_BUILD_LEGACY=ON` but are deprecated and scheduled for removal in 4.0.0.

---

## Architecture Overview

PvPGN 3.0.0 adopts **Domain-Driven Design (DDD)** with a **Hexagonal (Ports &
Adapters) Architecture**:

```
src/v3/
├── core/           # Shared kernel: StatusCode, contracts, formatting
├── domain/         # Pure domain model: Account, Channel, Game, Clan, …
├── application/    # Use cases + port interfaces (no infra dependencies)
├── infra/          # Adapters: SQLite, MySQL, PostgreSQL, file, shadow-write
├── integration/    # Strangler-fig bridges to legacy bnetd/d2cs/d2dbs
└── app/            # Entry points: bnetd-v3, pvpgn-migrate, pvpgn-config
```

**Layering rule**: `domain` → `application` → `infra`/`integration` → `app`.
No upward dependencies. Enforced by `scripts/v3_layering_check.sh` in CI.

---

## New Binaries

| Binary | Description |
|---|---|
| `bnetd-v3` | v3 Battle.net server daemon (replaces legacy `bnetd`) |
| `pvpgn-migrate` | CLI migration tool: `.plain` files → SQLite/TOML |
| `pvpgn-config` | Configuration validation and documentation tool |

---

## Configuration Migration

The primary configuration format is now **TOML** (`bnetd.toml`, `d2cs.toml`,
`d2dbs.toml`). Legacy `.conf` files are no longer read by the v3 binaries.

Quick migration:

```sh
./pvpgn-migrate --from conf/bnetd.conf --to conf/bnetd.toml
./pvpgn-migrate --accounts var/users/ --db var/pvpgn.sqlite
```

See [`docs/toml-migration.md`](toml-migration.md) for a full side-by-side
`.conf` → `.toml` walkthrough.

---

## Plugin System

PvPGN 3.0.0 ships a stable **Plugin C ABI 1.0** (`pvpgn/plugin/api.h`).
Plugins are shared libraries (`.so`/`.dll`) loaded at startup.

Key features:
- C ABI for maximum compatibility
- Optional seccomp sandbox (`-DPVPGN_V3_WITH_SECCOMP=ON`)
- Lua API v2 (`pvpgn.*` namespace) for scripted plugins
- Hot-reload support (SIGHUP)

See [`docs/plugin-migration.md`](plugin-migration.md) for migrating legacy Lua
scripts to the v2 API.

---

## Observability

- **Prometheus metrics** exposed at `http://localhost:9090/metrics` (configurable)
- **Health endpoints**: `/health/live` and `/health/ready`
- **OpenTelemetry tracing** (optional, `-DPVPGN_V3_WITH_OTLP=ON`)
- **Structured JSON logging** via spdlog (configurable sinks in `bnetd.toml`)

---

## Persistence Backends

| Backend | CMake flag | Notes |
|---|---|---|
| File (`.plain`) | always available | Legacy format, read-only in v3 |
| SQLite | `-DWITH_SQLITE3=ON` | Recommended for single-node |
| MySQL | `-DWITH_MYSQL=ON` | Production multi-node |
| PostgreSQL | `-DWITH_PGSQL=ON` | Production multi-node |

**Shadow-write** infrastructure allows running two backends in parallel during
migration (writes go to both; reads come from the primary).

---

## Build System

CMake presets are the recommended build interface:

| Preset | Description |
|---|---|
| `v3-dev` | Debug build, all v3 features |
| `v3-release` | Optimised release build |
| `v3-asan` | AddressSanitizer + UBSan |
| `v3-tsan` | ThreadSanitizer |
| `v3-coverage` | gcov/lcov coverage |
| `v3-fuzz` | libFuzzer targets |

```sh
cmake --preset v3-dev
cmake --build --preset v3-dev
ctest --preset v3-dev
```

---

## Breaking Changes

1. **`PVPGN_V3_BNETD_INTEGRATION` removed as a CMake option.** The v3
   integration layer is now always compiled in. The compile-time macro is still
   set to `1` on legacy targets when `PVPGN_BUILD_LEGACY=ON`.

2. **`PVPGN_BUILD_LEGACY` defaults to `OFF`.** New builds produce only v3
   binaries. Pass `-DPVPGN_BUILD_LEGACY=ON` to also build the legacy daemons.
   A deprecation warning is emitted at configure time.

3. **Project version bumped to `3.0.0`** (was `4.0.0` during development).

---

## Known Limitations

- The `pvpgn-config` binary is a stub in 3.0.0; full implementation is planned
  for 3.1.0.
- WOL (Westwood Online) protocol support in the v3 path is observation-only;
  full v3 WOL handlers are planned for 3.2.0.
- D2CS and D2DBS v3 integration is strangler-fig only; full v3 realm/character
  server is planned for 3.x.

---

## Deprecation Schedule

| Component | Deprecated in | Removal target |
|---|---|---|
| `src/bnetd/` legacy sources | 3.0.0 | 4.0.0 |
| `src/d2cs/` legacy sources | 3.0.0 | 4.0.0 |
| `src/d2dbs/` legacy sources | 3.0.0 | 4.0.0 |
| `src/compat/` | 3.0.0 | 4.0.0 |
| `PVPGN_BUILD_LEGACY` CMake option | 3.0.0 | 4.0.0 |

---

## Upgrade Path

1. Run `pvpgn-migrate` to convert `.plain` accounts to SQLite.
2. Run `pvpgn-migrate --config` to convert `bnetd.conf` to `bnetd.toml`.
3. Switch to `bnetd-v3` binary.
4. Migrate Lua plugins to the v2 API (see `docs/lua-api-v2.md`).
5. Optionally enable Prometheus metrics and OpenTelemetry tracing.
