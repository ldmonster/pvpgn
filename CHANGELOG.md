# Changelog

## [3.0.0] — 2026-05-29

### Breaking Changes
- `PVPGN_V3_BNETD_INTEGRATION` option removed; v3 integration is now mandatory
- `PVPGN_BUILD_LEGACY` now defaults to `OFF`; legacy binaries are opt-in

### Added
- Phase A–N: Full DDD + Hexagonal Architecture refactoring
- New v3 binaries: `bnetd-v3`, `pvpgn-migrate`, `pvpgn-config`
- Plugin C ABI 1.0 (`pvpgn/plugin/api.h`)
- Lua API v2 (`pvpgn.*` namespace)
- Prometheus metrics + health endpoints
- OpenTelemetry tracing (optional, `-DPVPGN_V3_WITH_OTLP=ON`)
- seccomp sandbox for plugins (optional, `-DPVPGN_V3_WITH_SECCOMP=ON`)
- SQLite, MySQL, PostgreSQL persistence backends
- Shadow-write migration infrastructure
- `pvpgn-migrate` CLI for `.plain` → SQLite/TOML migration
- CMakePresets: `v3-dev`, `v3-release`, `v3-asan`, `v3-tsan`, `v3-coverage`, `v3-fuzz`
- GitHub Actions CI: coverage, sanitizers, layering check, clang-tidy, docs

### Deprecated
- Legacy `src/bnetd/`, `src/d2cs/`, `src/d2dbs/` sources (scheduled for removal in 4.0.0)

### Migration Guide
See `docs/toml-migration.md` and `docs/lua-api-v2.md` for migration instructions.
