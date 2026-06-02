# Changelog

All notable changes are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/); see
[docs/developer/release-process.md](docs/developer/release-process.md) for the
versioning and deprecation policy.

## [Unreleased]

### Added
- Native plugin C ABI **purity gate** (`scripts/dev/check-plugin-abi-purity.sh`)
  proving `pvpgn/plugin/abi.h` exposes only pure C — no `domain/`/`application/`
  symbol can leak to a plugin (Plan 12).
- **Microbenchmark harness + regression gate** under `tests/bench/` with
  `scripts/dev/run-bench.sh` and `scripts/dev/check-bench-regression.py` (Plan 13).
- **Idle-connection memory-footprint** regression test (Plan 06).
- Capability-token **flat-map** lookup and a GCC14/Clang18 **CI compiler matrix**
  (Plan 09).
- **Mutation-testing pilot** over `domain/identity/` (weekly CI) (Plan 10).
- `[observability].sample_ratio` wired into the bnetd tracer at startup (Plan 11).
- New docs: `developer/release-process.md`, `developer/benchmarking.md`.

### Changed
- SQLite persistence consolidated onto the driver-parameterized
  `infra/persistence` repositories; all per-backend SQLite repos removed (Plan 07).
- The CLI tools and the SQLite backend build only when their C++23 `<print>` /
  `sqlite3` headers are available, skipping cleanly on an older toolchain (Plan 09).

### Removed
- The dead legacy crypto chain — `common/{bnethash,bnetsrp3,bigint,wolhash}`,
  the `infra/legacy_crypto` adapter, and its dead parity tests (Plan 08).

### Security
- Eliminated the last `std::rand()` use in `src/`; randomness in `src/` is now
  CSPRNG-only (`core::crypto::SecureRandom`) (Plan 08).

## [3.0.0] — 2026-05-29

### Added
- Phase A–N: Full DDD + Hexagonal Architecture refactoring
- New v3 binaries: `bnetd`, `pvpgn-migrate`, `pvpgn-config`
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

### Changed
- **Breaking:** `bnetd-v3` binary renamed to `bnetd`

### Deprecated
- Legacy `src/bnetd/`, `src/d2cs/`, `src/d2dbs/` sources (scheduled for removal in 4.0.0)

### Removed
- **Breaking:** `PVPGN_V3_BNETD_INTEGRATION` cmake option — v3 integration is now mandatory
- **Breaking:** `PVPGN_BUILD_LEGACY` cmake option — legacy build guards replaced
  with `if(TARGET common)` checks

**Migration:** see `docs/toml-migration.md` and `docs/lua-api-v2.md`.
