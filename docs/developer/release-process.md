# Release Process

This document codifies how PvPGN v3 is versioned, deprecates surface, and ships
releases. It is the policy referenced by the PR template and enforced (where
mechanical) by CI.

---

## Semantic Versioning

PvPGN follows [SemVer 2.0](https://semver.org/) — `MAJOR.MINOR.PATCH`. What bumps
which component is **defined by the compatibility of the operator- and
plugin-facing contracts**, not by how big the diff is:

| Change | Bump |
|--------|------|
| Battle.net / D2 **wire protocol** change (clients must change) | **major** |
| **Plugin ABI** `v1` → `v2` (`pvpgn/plugin/abi.h` breaking change) | **major** |
| **TOML schema** breaking change (a key removed or its meaning changed) | **major** |
| Removing a previously-deprecated config key / plugin hook / metric | **major** |
| New **opt-in** feature behind a flag or new config key (default off) | **minor** |
| New plugin capability / metric / Lua API (additive, back-compatible) | **minor** |
| Bug fix with no schema/ABI/wire change | **patch** |

The contracts that gate a **major** bump are pinned by tests so a breaking
change cannot ship unnoticed:

- Plugin ABI — `scripts/dev/check-plugin-abi.sh` (golden diff) +
  `scripts/dev/check-plugin-abi-purity.sh` (pure-C boundary).
- TOML schema — `infra/config` schema validator + `server_config_test`.
- Metric contract — `docs/operator/metrics.md` (the stable 3-metric contract).

The canonical version lives in the top-level `CMakeLists.txt` `project(... VERSION ...)`.

---

## Deprecation policy

Nothing operator- or plugin-facing is removed abruptly. Any config key, plugin
hook, metric, or CLI flag slated for removal must:

1. Be **announced in a MINOR release** and emit a **startup warning** when the
   deprecated surface is used (e.g. a removed-but-still-parsed config key logs a
   `WARN` naming its replacement).
2. Be **removed no earlier than the next MAJOR release** — a full major cycle of
   overlap.
3. Be recorded in `CHANGELOG.md` under a `### Deprecated` heading on announcement
   and under `### Removed` when finally removed.

---

## CHANGELOG discipline

`CHANGELOG.md` follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/):

- The newest section is `## [Unreleased]`; every user-facing change lands there
  as it merges, under one of the canonical headings: **Added, Changed,
  Deprecated, Removed, Fixed, Security**.
- Cutting a release renames `## [Unreleased]` to `## [x.y.z] — YYYY-MM-DD` and
  opens a fresh `## [Unreleased]` above it.
- `scripts/dev/check-changelog.sh` lints this in CI (an `## [Unreleased]`
  section exists, headings are from the canonical set, the Keep-a-Changelog
  reference is present).

---

## Release checklist

1. **Version** — bump `project(... VERSION ...)` in `CMakeLists.txt` per the
   table above.
2. **CHANGELOG** — rename `[Unreleased]` to the new version + date; open a fresh
   `[Unreleased]`.
3. **Pre-flight** — `bnetd --check-config` against the shipped sample config;
   run the full test suite + sanitizers + the microbench gate (`Plan 13`).
4. **Upgrade story** — confirm `docs/operator/runbooks/rolling-upgrade.md`
   covers any schema / at-rest-hash / I/O change in this release.
5. **Artefacts** *(pipeline-gated; tracked as Plan 15 follow-ups)* —
   - Distroless image (`gcr.io/distroless/cc-debian12`, non-root, < 80 MB);
     decision recorded in ADR `0011-runtime-image.md`.
   - Multi-arch `linux/amd64` + `linux/arm64` via `docker buildx`.
   - Sign binaries + images with cosign; publish the public key.
   - Attach a CycloneDX SBOM to the GitHub release.
6. **Tag** — `vX.Y.Z`; push; the release workflow publishes the signed artefacts.

---

## Rolling upgrades

Operators upgrade with no downtime by following
[`docs/operator/runbooks/rolling-upgrade.md`](../operator/runbooks/rolling-upgrade.md):
pre-flight config check, the at-rest-hash backward-compat window (Plan 08), TOML
schema versioning, and the health-probe contract during restart.
