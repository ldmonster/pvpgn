# 04 — d2cs / d2dbs Strangler

## What

Apply the same strangler-fig treatment to
`src/integration/legacy_d2cs/` and `src/integration/legacy_d2dbs/`
that plan 03 applies to `legacy_bnetd/`. End state: both directories
deleted; `d2cs` and `d2dbs` binaries built directly from v3 layers.

## Why

- These two trees were skipped in wave one because the `bnetd` payoff
  was bigger. They now block deleting half of `src/common/` (notably
  `d2cs_*_protocol.h` and `d2char_*`).
- They duplicate auth, session, and persistence patterns that already
  exist in cleaner form under `application/auth/`, `application/realm/`
  and `infra/persistence/`.

## Prerequisites

- Plan 03 done. The bridge pattern and tooling carry over.
- A domain context for the Realm aggregate already exists
  (`src/domain/realm/`) — verify it covers the d2cs/d2dbs use cases
  before starting.

## Concrete steps

1. **Map use cases.** For each public entry in `legacy_d2cs/` and
   `legacy_d2dbs/`, document which `application/` use case owns it
   (or create one). Output:
   `planstwo/inventory/d2-use-cases.csv`.
2. **Stand up v3 binaries.** Add `src/app/d2cs/` and `src/app/d2dbs/`
   modelled on `src/app/bnetd/`. Wire them to the existing
   `bnetd.toml` config sections `[d2cs]` and `[d2dbs]` (already in
   `conf/d2cs.toml.in` / `conf/d2dbs.toml.in`).
3. **Per-feature bridge then delete** (same loop as plan 03).
4. **Persistence.** Route d2dbs character storage through
   `infra/persistence/` (plan 07) rather than its own SQL layer.
5. **Delete** `src/integration/legacy_d2cs/` and
   `src/integration/legacy_d2dbs/`. Delete `pvpgn_d2cs_legacy` and
   `pvpgn_d2dbs_legacy` CMake targets.
6. **Move shared headers.** `d2char_checksum.{cpp,h}` → `core/d2/` if
   still needed; `d2cs_*_protocol.h`, `d2game_protocol.h`,
   `d2cs_d2dbs_ladder.h`, `d2cs_d2gs_*` → `src/protocol/d2/`.

## Acceptance criteria

- [ ] `src/integration/legacy_d2cs/` and `legacy_d2dbs/` deleted.
- [ ] `d2cs` and `d2dbs` binaries built from `src/app/d2cs/` and
      `src/app/d2dbs/` only.
- [ ] Round-trip integration test: a Diablo II client can log in,
      create a character, and save it on a v3-only build.
- [ ] No `src/common/d2*` file remains.
- [ ] Layering, `/WX`, sanitizers, full `ctest` green.

## Risks

- Character-save format is on-disk-stable. The persistence migration
  must read the legacy format and continue writing it (or emit a
  migration). Cover with a fixture-based round-trip test before any
  format change.
- d2gs (game server) is a third-party binary. Confirm the wire
  contract between d2cs ↔ d2gs has a regression test.

## Out of scope

- Changing the d2gs wire protocol.
- Adding new Diablo II features.
