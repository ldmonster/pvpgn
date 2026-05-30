# ADR 0004: Use Strangler Fig Pattern for Legacy bnetd Migration

**Date**: 2026-05-30  
**Status**: Accepted  
**Deciders**: PvPGN Core Team

## Context

PvPGN's core daemon (`bnetd`) is a ~150,000 LOC C/C++ monolith that has
accumulated 20+ years of technical debt.  A full rewrite was considered but
rejected for the following reasons:

- A "big bang" rewrite would require years of parallel development with no
  shippable intermediate state.
- The legacy code encodes subtle protocol behaviors (Battle.net quirks,
  game-specific handshakes) that are not fully documented and would be
  difficult to replicate correctly from scratch.
- The existing user base depends on the current behavior; a rewrite risks
  introducing regressions that break live servers.
- The team is small; maintaining two complete implementations simultaneously
  is not feasible.

Alternative approaches considered:

- **Module-by-module extraction without a bridge layer**: requires all
  modules to be extracted before any can be shipped; high risk.
- **Microservices split**: over-engineered for a single-process game server;
  adds network latency and operational complexity.
- **Strangler Fig Pattern**: incrementally replace subsystems while keeping
  the legacy code running; each replacement is independently shippable.

## Decision

Adopt the **Strangler Fig Pattern** to migrate the legacy `bnetd` monolith
to the v3 hexagonal architecture incrementally.

The migration proceeds as follows:

1. **Wrap**: the legacy `bnetd` code is compiled as a static library
   (`bnetd_legacy`) and linked into the v3 binary (`bnetd`).  The v3
   binary is the only shipping artifact; the legacy binary is deprecated.

2. **Bridge**: `src/integration/legacy_bnetd/` contains thin adapter classes
   (`pvpgn_v3_*_try` symbols) that delegate to the legacy library while
   exposing the v3 port interfaces.  These bridges are the "strangler fig"
   wrapping the old tree.

3. **Replace**: as each subsystem is re-implemented in `src/domain/` +
   `src/infra/`, the corresponding bridge is deleted and the legacy code
   path is removed.  Progress is tracked by `PVPGN_V3_COVERAGE` comments
   in `src/integration/legacy_bnetd/CMakeLists.txt`.

4. **Retire**: when `src/integration/legacy_bnetd/` shrinks to ≥ 80% LOC
   reduction, `PVPGN_BUILD_LEGACY` is removed and `bnetd-v3` is renamed
   `bnetd`.  *(As of 3.0.0: `PVPGN_BUILD_LEGACY` has been removed and the
   binary has been renamed `bnetd`.)*

Lifecycle bridge revisions are collapsed as they are superseded:
`_r246.cpp` + `_r247.cpp` were merged into `bnetd_lifecycle_bridges.cpp`
as the first example of this cleanup.

The `scripts/dev/retire-legacy.sh` script automates the final retirement
steps once the coverage threshold is met.

## Consequences

**Positive:**
- Every intermediate state is a shippable, working server; no "dark period"
  where the server is broken.
- The legacy code continues to handle edge cases while the v3 code is
  developed and tested.
- The bridge layer makes the migration boundary explicit and auditable.
- `cmake/layering_exceptions.txt` tracks known violations; its size is a
  measurable proxy for migration progress.

**Negative / Trade-offs:**
- The `integration/` layer is a deliberate architectural violation that must
  be cleaned up; it cannot be allowed to grow.
- `pvpgn_v3_*_try` symbols are a code smell; they must be removed once the
  corresponding subsystem is fully migrated.
- 80% LOC reduction in `integration/legacy_bnetd/` is a hard gate for
  retirement; progress must be tracked continuously.

**Resolved items (as of 3.0.0):**
- `PVPGN_BUILD_LEGACY` cmake option: **removed** — guards replaced with
  `if(TARGET common)` checks.
- `bnetd-v3` → `bnetd` rename: **completed** — CMake target renamed to
  `bnetd`; legacy target conflict resolved.

**Remaining items:**
- `pvpgn_v3_*_try` symbol removal: pending — strangler migration not yet
  100% complete; these symbols are the active bridge layer.
- xalloc/scoped_array/asnprintf deletion: pending — legacy integration code
  still references these headers.

**Related decisions:**
- ADR 0002 (Hexagonal Architecture) — the target architecture that the
  strangler migration is building toward.
