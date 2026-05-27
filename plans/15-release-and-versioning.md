# 15 — Release & Versioning Policy

**Goal:** Predictable releases, clear deprecation channel, no surprise
breakage for operators and plugin authors.

## 1. SemVer scope

Five independently-versioned surfaces, each documented:

| Surface | Version | Compatibility promise |
|---------|---------|-----------------------|
| Application (binaries: bnetd, d2cs, d2dbs) | `MAJOR.MINOR.PATCH` | SemVer for CLI flags and operator-visible behaviour |
| TOML config schema | `schema_version = N` | Forward-compatible reads within MAJOR; migrator for breaking changes |
| Database schema | `schema_version` row | Strict, see `07-persistence-and-migrations.md` |
| Plugin C ABI | `abi_major.abi_minor` | See `13-plugin-and-scripting.md` |
| Lua scripting API | `lua.api_version = N` | Major increments allow breaking changes; one release of overlap |

The application MAJOR can bump without the plugin ABI MAJOR bumping,
and vice versa. They're cross-referenced in the changelog.

## 2. Branching

- `main` — always green, always shippable.
- `release/MAJOR.MINOR` — created at RC time. Patch fixes cherry-picked.
- `legacy/3.x` (informal) — for the pre-DDD line; receives security
  fixes only until Phase E (`14-legacy-retirement.md`).

## 3. Release cadence

- Minor release every ~3 months when there are user-facing changes.
- Patch release as needed for security or crash bugs.
- Major release on schedule:
  - `v4.0`: end of Phase C (`PVPGN_V3_BNETD_INTEGRATION` mandatory,
    legacy off by default).
  - `v5.0`: end of Phase E (legacy deleted).

## 4. Changelog

- `CHANGES.md` (root) is the authoritative changelog. Conventional-
  commits–style entries, grouped under headings: `Added`, `Changed`,
  `Deprecated`, `Removed`, `Fixed`, `Security`.
- Each entry references the round number (`R210`) and PR.
- A short summary lands in `docs/releases/vMAJOR.MINOR.md`.

## 5. Deprecation policy

A feature goes through three states:

1. **Deprecated** — runtime warning on use, docs marked, planned
   removal version stated.
2. **Removed** — in the planned version, code deleted; the
   `Removed` section of the changelog points to the migration.
3. Minimum window: one MINOR release between deprecation and
   removal for config/scripting/plugin surfaces; one MAJOR for
   anything that breaks builds.

## 6. Security

- `SECURITY.md` at repo root: how to report, response window.
- CVE-eligible bugs trigger a coordinated release across all
  supported branches.
- Audit log (see `08-error-handling-and-logging.md` §5) is the
  forensic baseline.

## 7. Artefacts

Each release publishes:

- Source tarball.
- Linux x86_64 static binary (musl, from the existing Alpine build).
- Windows x64 zip (MSVC, static vcpkg).
- macOS arm64 + x86_64 binaries.
- Docker images (`pvpgn/bnetd`, `pvpgn/d2cs`, `pvpgn/d2dbs`,
  `pvpgn/v3-compose` for the all-in-one stack).
- SBOM (CycloneDX) generated from vcpkg + system deps.
- SHA256SUMS + GPG signature.

## 8. Concrete tasks

- [ ] R307: write `SECURITY.md`, `RELEASING.md`.
- [ ] R308: changelog automation (Conventional Commits → changelog
      section).
- [ ] R309: SBOM generation in CI release workflow.
- [ ] R310: signed release artefacts.
