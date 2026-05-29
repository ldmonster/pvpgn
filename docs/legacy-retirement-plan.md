# Legacy Retirement Plan

## Overview

This document describes the plan for retiring the legacy PvPGN sources
(`src/bnetd/`, `src/d2cs/`, `src/d2dbs/`, `src/compat/`) and promoting the
v3 sub-tree (`src/v3/`) to the top-level `src/` directory.

**Timeline**: Scheduled for PvPGN **4.0.0**.

The retirement script is at [`scripts/dev/retire-legacy.sh`](../scripts/dev/retire-legacy.sh).

---

## What Will Be Deleted

The following directories will be removed entirely:

| Directory | Contents |
|---|---|
| `src/bnetd/` | Legacy Battle.net daemon sources (~200 files) |
| `src/d2cs/` | Legacy Diablo 2 Character Server sources |
| `src/d2dbs/` | Legacy Diablo 2 Database Server sources |
| `src/compat/` | Legacy POSIX/Win32 compatibility shims |

Also removed:
- `src/common/` — legacy shared utilities (replaced by `src/v3/core/`)
- `src/win32/` — legacy Win32 GUI sources

---

## What Will Be Moved

The v3 sub-tree will be promoted from `src/v3/` to `src/`:

```
src/v3/core/           → src/core/
src/v3/domain/         → src/domain/
src/v3/application/    → src/application/
src/v3/infra/          → src/infra/
src/v3/integration/    → src/integration/
src/v3/app/            → src/app/
src/v3/tools/          → src/tools/
```

---

## CMakeLists.txt Changes Required After the Move

After running `retire-legacy.sh --apply`, the following CMake changes are needed:

1. **Root `CMakeLists.txt`**:
   - Remove `option(PVPGN_BUILD_LEGACY ...)` and all `if(PVPGN_BUILD_LEGACY)` blocks
   - Remove `include(ConfigureChecks.cmake)` (legacy-only)
   - Change `add_subdirectory(src/v3)` → `add_subdirectory(src)`
   - Remove `add_subdirectory(src/v3/integration/legacy_bnetd)` etc.
   - Remove `add_subdirectory(conf)`, `add_subdirectory(files)`, `add_subdirectory(man)` (legacy install targets)

2. **`src/CMakeLists.txt`** (new, replacing the legacy one):
   - Add `add_subdirectory(core)`
   - Add `add_subdirectory(domain)`
   - Add `add_subdirectory(application)`
   - Add `add_subdirectory(infra)`
   - Add `add_subdirectory(integration)`
   - Add `add_subdirectory(app)`
   - Add `add_subdirectory(tools)`

---

## Include Path Changes Required

After the move, all `#include` paths that reference `src/v3/` sub-paths will
need updating. The primary change is:

```cpp
// Before (in legacy files that include v3 headers):
#include "integration/legacy_bnetd/prefs_bridge.hpp"

// After (no change needed — relative includes within src/ still work):
#include "integration/legacy_bnetd/prefs_bridge.hpp"
```

Most v3-internal includes use relative paths and will not need changes.
The `src/v3/` prefix in CMake `target_include_directories` calls will need
updating to `src/`.

---

## How to Use the Retirement Script

### Dry run (safe — no changes made)

```sh
./scripts/dev/retire-legacy.sh
```

This prints every file/directory that would be deleted or moved, without
making any changes.

### Apply (IRREVERSIBLE)

```sh
# Commit everything first!
git add -A && git commit -m "chore: pre-retirement checkpoint"

./scripts/dev/retire-legacy.sh --apply

# Then update CMakeLists.txt and include paths as described above
cmake --preset v3-dev
cmake --build --preset v3-dev
ctest --preset v3-dev

git add -A && git commit -m "chore: retire legacy sources (R354)"
```

---

## Checklist for 4.0.0 Retirement

- [ ] All legacy `#ifdef PVPGN_V3_BNETD_INTEGRATION` call sites have been
      verified as always-on (no legacy-only fallback paths remain)
- [ ] `pvpgn-migrate` can fully migrate all legacy `.plain` account files
- [ ] All CI presets pass with `PVPGN_BUILD_LEGACY=OFF` (already the default)
- [ ] Release notes for 4.0.0 document the removal
- [ ] Run `./scripts/dev/retire-legacy.sh --apply`
- [ ] Update `CMakeLists.txt` paths
- [ ] Update `#include` paths
- [ ] All tests pass: `ctest --preset v3-dev`
- [ ] Tag `v4.0.0`

---

## Rationale

The strangler-fig migration (Phases A–N, R210–R354) is complete. The v3
DDD + Hexagonal Architecture is the primary codebase. The legacy sources
exist only as a safety net for operators who have not yet migrated. Once
`pvpgn-migrate` has been validated in production and the v3 binaries have
been in use for a full release cycle, the legacy sources can be safely removed.
