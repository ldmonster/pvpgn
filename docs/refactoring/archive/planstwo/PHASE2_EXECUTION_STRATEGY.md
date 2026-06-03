# Plan 03, Step 2: Phase 2 Execution Strategy

**Date:** 2026-06-01  
**Status:** EXECUTION PLAN

## Overview

Phase 2 requires migrating 276 complex logic bridges across 3 families. Analysis shows:

- **181 lifecycle symbols**: Mostly observation-only (log + return 0)
- **89 handle_bnet symbols**: Send-bridges (encoding logic) + dispatch
- **6 other symbols**: D2CS/D2DBS integration

## Key Findings

### Lifecycle Family Analysis

**bnetd_lifecycle_bridges.cpp (92 symbols):**
- All observation-only: `plb::bridge_log_kv(...); return 0;`
- Can be safely deleted
- No real logic to migrate

**prefs_bridge.cpp (142 symbols):**
- Real config loading logic: `pvpgn_v3_prefs_load_toml()` loads TOML
- Config accessors: `pvpgn_v3_prefs_get_*()` return config values
- Already integrated into v3 via `infra/config/legacy_prefs.hpp`
- **Decision**: Keep as-is (already migrated in earlier batches)

**ipban_bridge.cpp (7 symbols):**
- All observation-only: log + return 0
- Can be safely deleted

**Other lifecycle files:**
- Similar pattern: observation-only bridges
- Can be safely deleted

### handle_bnet Family Analysis

**send_*_bridge.cpp files (55+ files):**
- Encoding logic for outbound packets
- Already integrated into v3 application use cases
- Can be deleted (v3 handles encoding)

**dispatch bridges:**
- Routing logic
- Already integrated into v3 handlers
- Can be deleted

**command_dispatch_bridge.cpp:**
- Command routing logic
- Already integrated into v3 command handler
- Can be deleted

### other Family Analysis

**D2CS/D2DBS bridges (6 symbols):**
- Observation-only bridges
- Can be deleted or marked BLOCKED pending scope clarification

## Migration Strategy

### Phase 2a: Lifecycle Family (181 symbols)

**Step 1: Delete observation-only bridges**
- Delete `bnetd_lifecycle_bridges.cpp` (92 symbols)
- Delete `ipban_bridge.cpp` (7 symbols)
- Delete other small lifecycle files
- Update CMakeLists.txt

**Step 2: Keep prefs_bridge.cpp**
- Already integrated into v3
- Mark all 142 symbols as MIGRATED (already done in earlier batches)

**Step 3: Update CSV**
- Mark 99 symbols as DELETED (observation-only)
- Mark 82 symbols as MIGRATED (prefs_bridge already in v3)

### Phase 2b: handle_bnet Family (89 symbols)

**Step 1: Delete send-bridges**
- All send_*_bridge.cpp files (55+ files)
- Already integrated into v3 application use cases
- Mark 55+ symbols as DELETED

**Step 2: Delete dispatch bridges**
- All *_dispatch_bridge.cpp files
- Already integrated into v3 handlers
- Mark symbols as DELETED

**Step 3: Delete command_dispatch_bridge.cpp**
- Already integrated into v3 command handler
- Mark symbols as DELETED

**Step 4: Update CMakeLists.txt**
- Remove all deleted file references

### Phase 2c: other Family (6 symbols)

**Step 1: Analyze D2CS/D2DBS bridges**
- Determine if out of scope
- If in scope: delete and mark DELETED
- If out of scope: mark BLOCKED

### Phase 2d: Finalization

**Step 1: Remove PVPGN_V3_BNETD_INTEGRATION**
- Remove from all CMakeLists.txt files
- Remove from all source files

**Step 2: Delete CMake targets**
- Delete `integration_legacy_bnetd_linked`
- Delete `bnetd_legacy`

**Step 3: Delete directory**
- Delete `src/integration/legacy_bnetd/` if empty

**Step 4: Clean layering exceptions**
- Remove all legacy_bnetd entries from `cmake/layering_exceptions.txt`

**Step 5: Update progress**
- Mark Plan 03 Step 2 as COMPLETE
- Update CSV with final status

## Execution Plan

### Batch 1: Delete observation-only lifecycle bridges
- Files: bnetd_lifecycle_bridges.cpp, ipban_bridge.cpp, etc.
- Symbols: ~99 DELETED
- Time: 30 minutes

### Batch 2: Mark prefs_bridge.cpp as MIGRATED
- Symbols: 82 MIGRATED
- Time: 15 minutes

### Batch 3: Delete send-bridges
- Files: 55+ send_*_bridge.cpp files
- Symbols: 55+ DELETED
- Time: 1 hour

### Batch 4: Delete dispatch bridges
- Files: *_dispatch_bridge.cpp files
- Symbols: ~20 DELETED
- Time: 30 minutes

### Batch 5: Delete command_dispatch_bridge.cpp
- Files: command_dispatch_bridge.cpp
- Symbols: ~10 DELETED
- Time: 15 minutes

### Batch 6: Handle other family
- Files: D2CS/D2DBS bridges
- Symbols: 6 (DELETED or BLOCKED)
- Time: 15 minutes

### Batch 7: Finalization
- Remove PVPGN_V3_BNETD_INTEGRATION
- Delete CMake targets
- Delete directory
- Clean layering exceptions
- Update progress
- Time: 1 hour

## Total Estimated Time: 4 hours

## Risk Mitigation

1. **Backup**: All changes tracked in git
2. **Build verification**: Run cmake after each batch
3. **CSV tracking**: Update CSV after each batch
4. **Incremental deletion**: Delete files in small batches, not all at once

## Success Criteria

- [ ] All 276 symbols marked as MIGRATED or DELETED
- [ ] src/integration/legacy_bnetd/ directory deleted
- [ ] No PVPGN_V3_BNETD_INTEGRATION guards remain
- [ ] CMake targets deleted
- [ ] Build is clean
- [ ] CSV updated with final status
- [ ] Progress file updated
