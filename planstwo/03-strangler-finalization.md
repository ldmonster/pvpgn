# 03 — Strangler Finalization: `src/integration/legacy_bnetd/`

## What

Reduce `src/integration/legacy_bnetd/` to zero source files. The
directory is deleted. All behaviour lives in `src/{domain,application,
infra,integration/bnet}`.

## Why

- Wave one shrank the legacy tree and renamed the binary, but the
  ≈ 170 TUs still in `legacy_bnetd/` are the single biggest source of
  warnings, build time, and `src/common/` dependencies.
- Until this is empty, plans 02 (common purge), 06 (async I/O), and
  09 (C++23) are partially blocked.

## Prerequisites

- Wave-one plan 06 (strangler completion) shipped the per-feature
  `pvpgn_v3_*` bridges. Wave two finishes the migration in the
  opposite direction: v3 owns the entry point, legacy fragments are
  inlined into the v3 handlers and deleted.

## Concrete steps

1. **Bridge inventory.** Generate
   `planstwo/inventory/bridge-symbols.csv` listing every
   `pvpgn_v3_<op>` symbol exported by
   `integration_legacy_bnetd_linked`, with: legacy caller, v3 callee,
   LOC of legacy caller, test coverage.
2. **Group by handler family** matching the wave-one decomposition
   (`handle_bnet/`, `handle_wol/`, `irc/`, etc.). One PR per family.
3. **Per family, repeat:**
   - Move the legacy handler body into the v3 use case it bridges to.
   - Delete the legacy `.cpp` and its header.
   - Delete the `extern "C"` bridge symbol.
   - Drop the `#ifdef PVPGN_V3_BNETD_INTEGRATION` guard at the call
     site — the call site is now the v3 handler directly.
   - Add unit tests for any behaviour previously only exercised
     through the legacy path.
4. **Lifecycle.** Fold
   `bnetd_lifecycle_bridges.cpp` into `src/app/bnetd/main.cpp` /
   `src/runtime/lifecycle.cpp`. Delete the bridge.
5. **Build target removal.** Delete
   `integration_legacy_bnetd_linked` and `bnetd_legacy` CMake targets.
   Remove `PVPGN_V3_BNETD_INTEGRATION` definition.
6. **Tree removal.** `git rm -r src/integration/legacy_bnetd/`.
7. **Layering exceptions.** Delete every legacy-bnetd entry from
   `cmake/layering_exceptions.txt`.

## Acceptance criteria

- [ ] `src/integration/legacy_bnetd/` does not exist.
- [ ] `git grep -l 'pvpgn_v3_.*_try\|PVPGN_V3_BNETD_INTEGRATION' src/`
      returns nothing.
- [ ] `bnetd_legacy` and `integration_legacy_bnetd_linked` CMake
      targets are deleted.
- [ ] `cmake/layering_exceptions.txt` has no `legacy_bnetd` entries.
- [ ] Layering, `/WX`, sanitizer matrix, and full `ctest` green.

## Risks

- Behavioural drift: legacy handlers contain edge cases (e.g. WoL
  fallback paths) not exercised by unit tests. Mitigate by recording
  a packet capture of a real client session and replaying it against
  the v3-only build as an e2e test before deleting each family.
- Lua hooks called from legacy handlers must continue firing from the
  v3 handlers. The Lua API v2 conformance test catches missed hooks.

## Out of scope

- Renaming v3 directories.
- Touching `legacy_d2cs` / `legacy_d2dbs` (plan 04).
