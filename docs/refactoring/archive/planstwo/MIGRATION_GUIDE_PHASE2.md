# Plan 03, Step 2: Per-Family Handler Migration — Phase 2 Guide

## Overview

**Phase 1 Status: COMPLETE**
- ✅ Identified 49 thin wrapper bridge files
- ✅ Deleted 49 thin wrapper files from `src/integration/legacy_bnetd/src/`
- ✅ Updated `src/integration/legacy_bnetd/CMakeLists.txt` to remove deleted file references
- ✅ Updated `planstwo/inventory/bridge-symbols.csv` with DELETED status for 61 symbols

**Remaining Work: Phase 2 (Complex Logic Migration)**
- 276 symbols remain with complex logic that requires migration
- These symbols are distributed across 5 handler families

## Current Status by Family

| Family | Total Symbols | Deleted (Phase 1) | Remaining | Status |
|--------|---------------|-------------------|-----------|--------|
| lifecycle | 192 | 11 | 181 | PENDING |
| handle_bnet | 135 | 46 | 89 | PENDING |
| handle_wol | 1 | 1 | 0 | COMPLETE |
| irc | 1 | 1 | 0 | COMPLETE |
| other | 8 | 2 | 6 | PENDING |
| **TOTAL** | **337** | **61** | **276** | **PENDING** |

## Phase 2: Complex Logic Migration Strategy

### For Each Remaining Symbol

The migration process for each symbol follows this pattern:

1. **Analyze the bridge implementation**
   - Read the bridge function in `src/integration/legacy_bnetd/src/<family>/<file>.cpp`
   - Determine if it's a thin wrapper or contains real logic
   - Identify the v3 callee from the CSV

2. **Categorize the logic**
   - **Type A: Thin wrapper** → Delete the bridge, v3 already handles it
   - **Type B: Observation-only** → Delete the bridge, legacy falls through
   - **Type C: Real logic** → Move logic to v3 use case, delete bridge
   - **Type D: Hybrid** → Extract real logic to v3, keep thin wrapper for compatibility

3. **For Type C/D (Real Logic)**
   - Locate the v3 use case file: `src/application/<context>/src/<use_case>.cpp`
   - Extract the logic from the bridge
   - Integrate it into the v3 use case
   - Add comment: `// MIGRATED from legacy_bnetd/<file>.cpp`
   - Update any #ifdef guards

4. **Delete the bridge**
   - Remove the `extern "C"` function definition
   - Remove the bridge file from CMakeLists.txt
   - Remove the header file from `include/integration/legacy_bnetd/`

5. **Update call sites**
   - Remove `#ifdef PVPGN_V3_BNETD_INTEGRATION` guards
   - Call the v3 function directly
   - Update any error handling

6. **Update CSV**
   - Mark symbol as MIGRATED or DELETED
   - Add notes about what was done

### Family-by-Family Breakdown

#### lifecycle (181 remaining symbols)

**Key files to migrate:**
- `bnetd_lifecycle_bridges.cpp` (92 symbols) — observation-only lifecycle hooks
- `prefs_bridge.cpp` (89 symbols) — configuration accessors
- Other small lifecycle files

**Strategy:**
1. Most lifecycle symbols are observation-only (return 0; legacy falls through)
2. These can be deleted immediately
3. `prefs_bridge.cpp` contains real logic for config loading/access
   - Move to `src/application/init/src/` or `src/runtime/`
   - Integrate with v3 config system

**Estimated effort:** 3-4 PRs

#### handle_bnet (89 remaining symbols)

**Key files to migrate:**
- `command_dispatch_bridge.cpp` (350 LOC) — command routing
- `send_*_bridge.cpp` files (55+ files) — packet encoding
- `*_dispatch_bridge.cpp` files — protocol dispatch

**Strategy:**
1. Send-bridges are mostly encoding logic → move to `src/application/<context>/src/`
2. Dispatch bridges are routing logic → move to appropriate handler
3. Command dispatch is complex → requires careful refactoring

**Estimated effort:** 5-7 PRs

#### handle_wol (0 remaining symbols)

**Status:** ✅ COMPLETE (1 symbol deleted in Phase 1)

#### irc (0 remaining symbols)

**Status:** ✅ COMPLETE (1 symbol deleted in Phase 1)

#### other (6 remaining symbols)

**Key files:**
- `d2cs_link_dispatch_bridge.cpp` (2 symbols)
- `d2_character_dispatch_bridge.cpp` (1 symbol)
- `handle_d2cs_link.cpp` (3 symbols)

**Strategy:**
1. These are D2CS/D2DBS integration points
2. May be out of scope for Plan 03 (see plan document)
3. Verify with team before migrating

**Estimated effort:** 1-2 PRs

## Implementation Checklist

### Phase 2a: Lifecycle Family
- [ ] Analyze `bnetd_lifecycle_bridges.cpp` (92 symbols)
- [ ] Delete observation-only bridges
- [ ] Migrate `prefs_bridge.cpp` (89 symbols)
- [ ] Update CMakeLists.txt
- [ ] Update CSV with MIGRATED/DELETED status
- [ ] Create PR: "Plan 03 Step 2: Migrate lifecycle family"

### Phase 2b: handle_bnet Family
- [ ] Analyze send-bridges (55+ files)
- [ ] Migrate send-bridges to application use cases
- [ ] Analyze dispatch-bridges
- [ ] Migrate dispatch-bridges
- [ ] Analyze command_dispatch_bridge.cpp
- [ ] Migrate command dispatch logic
- [ ] Update CMakeLists.txt
- [ ] Update CSV with MIGRATED/DELETED status
- [ ] Create PR: "Plan 03 Step 2: Migrate handle_bnet family"

### Phase 2c: other Family
- [ ] Verify scope with team (D2CS/D2DBS)
- [ ] Migrate or defer based on decision
- [ ] Update CSV with status
- [ ] Create PR if migrated

### Phase 2d: Finalization
- [ ] Delete `integration_legacy_bnetd_linked` CMake target
- [ ] Delete `bnetd_legacy` CMake target
- [ ] Remove `PVPGN_V3_BNETD_INTEGRATION` definition from all CMakeLists.txt
- [ ] Delete `src/integration/legacy_bnetd/` directory
- [ ] Clean `cmake/layering_exceptions.txt`
- [ ] Update `refactoring-progress-wave2.md`
- [ ] Create PR: "Plan 03 Step 2: Delete legacy_bnetd directory"

## Key Files to Monitor

- `src/integration/legacy_bnetd/CMakeLists.txt` — Remove deleted files
- `src/integration/legacy_bnetd/include/` — Delete headers as bridges are removed
- `src/integration/legacy_bnetd/src/` — Delete bridge implementations
- `planstwo/inventory/bridge-symbols.csv` — Track migration status
- `cmake/layering_exceptions.txt` — Remove legacy_bnetd entries
- `refactoring-progress-wave2.md` — Update progress

## Testing Strategy

For each family migration:

1. **Unit tests** — Add tests for migrated logic in v3 use cases
2. **Integration tests** — Verify bridge removal doesn't break build
3. **E2E tests** — Replay packet captures from real client sessions
4. **Lua API tests** — Verify Lua hooks still fire from v3 handlers

## Risk Mitigation

- **Behavioral drift:** Record packet captures before deleting each family
- **Lua hooks:** Run Lua API v2 conformance test after each family
- **Build breaks:** Update CMakeLists.txt immediately after file deletion
- **Incomplete migration:** Mark symbols as PENDING until fully migrated

## Success Criteria

- [ ] All 276 remaining symbols are either MIGRATED or DELETED
- [ ] `src/integration/legacy_bnetd/` directory is empty and deleted
- [ ] No `pvpgn_v3_*` symbols remain in codebase
- [ ] No `PVPGN_V3_BNETD_INTEGRATION` guards remain
- [ ] `cmake/layering_exceptions.txt` has no legacy_bnetd entries
- [ ] Build is clean (no warnings, no errors)
- [ ] All tests pass (unit, integration, E2E, Lua)
- [ ] `refactoring-progress-wave2.md` shows Plan 03 Step 2 as ✅ COMPLETE

## Next Steps

1. **Immediate:** Review this guide with the team
2. **Week 1:** Complete Phase 2a (lifecycle family)
3. **Week 2:** Complete Phase 2b (handle_bnet family)
4. **Week 3:** Complete Phase 2c (other family) and Phase 2d (finalization)
5. **Week 4:** Testing, bug fixes, and documentation

## References

- `plans/03-strangler-finalization.md` — Original plan
- `planstwo/inventory/bridge-symbols.csv` — Bridge inventory
- `scripts/dev/migrate_bridges_phase1.py` — Phase 1 automation
- `scripts/dev/strip_v3_guards.py` — Guard removal utility
