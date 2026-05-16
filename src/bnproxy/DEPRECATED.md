# bnproxy — DEPRECATED

**Status:** Removed in PvPGN 4.0

## Overview

The `bnproxy` tool is a legacy proxy utility that is no longer maintained or built as part of the PvPGN project. It has been excluded from the CMake build system as of version 4.0.

## Rationale

- The proxy functionality is superseded by modern networking infrastructure in the v3 refactoring
- Maintenance burden outweighs usage
- No active users or use cases identified

## Migration Path

If you require proxy functionality, consider:
1. Using the v3 refactored networking layer (see `src/v3/protocol/`)
2. Implementing custom proxy logic using the v3 infrastructure
3. Using third-party proxy solutions (e.g., HAProxy, nginx)

## Source Code

The source code remains in this directory for historical reference:
- `bnproxy.c` — Main proxy implementation
- `virtconn.c` / `virtconn.h` — Virtual connection handling

To build this tool manually (not recommended):
```bash
# This is not supported; the tool is deprecated
# Refer to git history if you need to understand the implementation
```

## Questions?

Refer to the main refactoring documentation:
- `refactoring-plan-15-migration-roadmap.md` — Phase 9 cleanup details
- `refactoring-progress.md` — Overall refactoring status
