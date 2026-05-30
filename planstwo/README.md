# PvPGN Refactoring Plan — Wave Two

This `planstwo/` folder is the **next** wave of refactoring work, layered on
top of [`plans/`](../plans/README.md). Wave one (`plans/`) covered the
v3 hexagonal-architecture stand-up, repo hygiene, TOML consolidation,
plugin/observability surfaces and most of the strangler-fig over the
legacy `bnetd` tree. Many of those items are already merged — see
[`refactoring-progress.md`](../refactoring-progress.md).

Wave two targets:

1. **Finish the strangler.** Delete `src/integration/legacy_bnetd/`,
   `legacy_d2cs/`, `legacy_d2dbs/`, and the matching `src/common/*`
   utilities that only exist to feed them.
2. **Modernize the foundation.** Replace hand-rolled `fdwatch`,
   `hashtable`, `xstring`, custom crypto, and bespoke config parsers
   with the C++23 standard library and a small set of vetted vendor
   libraries.
3. **Harden delivery.** Sanitizer matrix, fuzz CI gate, performance
   regression gate, OpenTelemetry, signed plugin ABI, distroless image,
   documented rolling-upgrade path.

Read the files in order. Each is independently actionable and ends with
a checklist suitable for ticking off in `refactoring-progress.md`.

| # | File | Theme |
|---|------|-------|
| 00 | [overview.md](00-overview.md) | Where we are, where we are going |
| 01 | [principles-and-acceptance.md](01-principles-and-acceptance.md) | Definition of Done for wave two |
| 02 | [common-purge.md](02-common-purge.md) | Retire `src/common/*` |
| 03 | [strangler-finalization.md](03-strangler-finalization.md) | Delete `src/integration/legacy_bnetd/` |
| 04 | [d2cs-d2dbs-strangler.md](04-d2cs-d2dbs-strangler.md) | Same treatment for d2cs / d2dbs |
| 05 | [ports-consolidation.md](05-ports-consolidation.md) | Collapse stray `application/ports/` |
| 06 | [async-io-modernization.md](06-async-io-modernization.md) | Replace `fdwatch` with a real async runtime |
| 07 | [infra-adapter-rehab.md](07-infra-adapter-rehab.md) | Unify sqlite / mysql / postgres adapters |
| 08 | [crypto-modernization.md](08-crypto-modernization.md) | Argon2id, libsodium, vetted SRP |
| 09 | [cpp23-uplift.md](09-cpp23-uplift.md) | `std::expected`, `std::print`, modules |
| 10 | [testing-pyramid-completion.md](10-testing-pyramid-completion.md) | Sanitizers, fuzz gate, coverage gate |
| 11 | [observability-otel.md](11-observability-otel.md) | Real OpenTelemetry exporter |
| 12 | [plugin-abi-stabilization.md](12-plugin-abi-stabilization.md) | Semver'd C plugin ABI |
| 13 | [perf-benchmark-baseline.md](13-perf-benchmark-baseline.md) | Bench harness + CI regression gate |
| 14 | [docs-and-mkdocs-strict.md](14-docs-and-mkdocs-strict.md) | `mkdocs build --strict` green |
| 15 | [release-and-rollout.md](15-release-and-rollout.md) | Semver, migrations, deprecation policy |
| 16 | [execution-roadmap.md](16-execution-roadmap.md) | Ordering, milestones, rollback |

Track progress in `refactoring-progress.md` under a new `## Wave Two`
header.
