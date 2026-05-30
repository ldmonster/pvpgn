#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# retire-legacy.sh — Dry-run / apply script for the final legacy retirement step.
#
# Updated 2026-05-30 to reflect the current directory layout:
#   - src/bnetd/ is already EMPTY (legacy tree deletion criterion is met).
#   - src/v3/ does NOT exist; v3 sources live directly under src/.
#   - The remaining retirement work is removing CMake targets and bridge files,
#     not moving directories.
#
# What this script now checks / performs:
#   Step 1: Verify src/bnetd/ is empty (already met).
#   Step 2: Verify src/v3/ does not exist (already met — no move needed).
#   Step 3: Report remaining retirement blockers (CMake targets, bridge files).
#   Step 4: (--apply) Remove empty stub files left by lifecycle bridge collapse.
#
# Usage:
#   ./scripts/dev/retire-legacy.sh          # dry run (default)
#   ./scripts/dev/retire-legacy.sh --apply  # actually perform the changes
#
# WARNING: --apply is IRREVERSIBLE. Commit everything first.
#
# Full retirement requires completing Plan 06 phases 1-7 first:
#   plans/16-strangler-completion-detail.md
set -euo pipefail

DRY_RUN=true
if [[ "${1:-}" == "--apply" ]]; then
    DRY_RUN=false
fi

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

echo "=== PvPGN Legacy Retirement Script ==="
echo "Mode: $([ "$DRY_RUN" = true ] && echo 'DRY RUN' || echo 'APPLY')"
echo "Repo: $REPO_ROOT"
echo ""

# ---------------------------------------------------------------------------
# Step 1: Verify src/bnetd/ is empty (legacy tree deletion criterion)
# ---------------------------------------------------------------------------
echo "--- Step 1: src/bnetd/ legacy tree ---"
BNETD_DIR="$REPO_ROOT/src/bnetd"
if [[ ! -d "$BNETD_DIR" ]]; then
    echo "  OK: src/bnetd/ does not exist (already deleted)"
elif [[ -z "$(find "$BNETD_DIR" -maxdepth 1 -name '*.cpp' -o -name '*.h' -o -name '*.c' 2>/dev/null | head -1)" ]]; then
    echo "  OK: src/bnetd/ exists but contains no source files (criterion met)"
    if [[ "$DRY_RUN" = false ]]; then
        echo "  NOTE: src/bnetd/ is empty; nothing to delete."
    fi
else
    echo "  BLOCKER: src/bnetd/ still contains source files:"
    find "$BNETD_DIR" -name '*.cpp' -o -name '*.h' -o -name '*.c' | sort | sed 's/^/    /'
fi

echo ""

# ---------------------------------------------------------------------------
# Step 2: Verify src/v3/ does not exist (no directory move needed)
# ---------------------------------------------------------------------------
echo "--- Step 2: src/v3/ directory ---"
V3_DIR="$REPO_ROOT/src/v3"
if [[ ! -d "$V3_DIR" ]]; then
    echo "  OK: src/v3/ does not exist (v3 sources already live under src/)"
else
    echo "  INFO: src/v3/ exists — check if it still contains sources:"
    find "$V3_DIR" -name '*.cpp' -o -name '*.h' | head -10 | sed 's/^/    /'
fi

echo ""

# ---------------------------------------------------------------------------
# Step 3: Report remaining retirement blockers
# ---------------------------------------------------------------------------
echo "--- Step 3: Remaining retirement blockers ---"

# Check for PVPGN_BUILD_LEGACY references
LEGACY_GUARD_COUNT=$(grep -r "PVPGN_BUILD_LEGACY" "$REPO_ROOT/src" --include="CMakeLists.txt" -l 2>/dev/null | wc -l)
if [[ "$LEGACY_GUARD_COUNT" -gt 0 ]]; then
    echo "  BLOCKER: PVPGN_BUILD_LEGACY still referenced in $LEGACY_GUARD_COUNT CMakeLists.txt file(s):"
    grep -r "PVPGN_BUILD_LEGACY" "$REPO_ROOT/src" --include="CMakeLists.txt" -l | sed 's/^/    /'
else
    echo "  OK: No PVPGN_BUILD_LEGACY references in src/ CMakeLists.txt files"
fi

# Check for pvpgn_v3_*_try symbols in source
TRY_SYMBOL_COUNT=$(grep -r "pvpgn_v3_.*_try" "$REPO_ROOT/src" --include="*.cpp" --include="*.h" --include="*.hpp" -l 2>/dev/null | grep -v "integration/legacy_bnetd" | wc -l)
if [[ "$TRY_SYMBOL_COUNT" -gt 0 ]]; then
    echo "  BLOCKER: pvpgn_v3_*_try symbols still referenced outside integration/legacy_bnetd/ ($TRY_SYMBOL_COUNT file(s)):"
    grep -r "pvpgn_v3_.*_try" "$REPO_ROOT/src" --include="*.cpp" --include="*.h" --include="*.hpp" -l | grep -v "integration/legacy_bnetd" | sed 's/^/    /'
else
    echo "  OK: No pvpgn_v3_*_try references outside integration/legacy_bnetd/"
fi

# Check for bnetd_legacy CMake target
if grep -r "bnetd_legacy" "$REPO_ROOT/src" --include="CMakeLists.txt" -q 2>/dev/null; then
    echo "  BLOCKER: bnetd_legacy CMake target still referenced in src/ CMakeLists.txt"
else
    echo "  OK: No bnetd_legacy references in src/ CMakeLists.txt"
fi

# Check for pvpgn_v3_bnetd rename
if grep -r "add_executable(pvpgn_v3_bnetd" "$REPO_ROOT/src" --include="CMakeLists.txt" -q 2>/dev/null; then
    echo "  TODO: pvpgn_v3_bnetd executable not yet renamed to bnetd"
    echo "        (blocked by legacy bnetd target conflict — see plans/16-strangler-completion-detail.md §7.4)"
else
    echo "  OK: pvpgn_v3_bnetd has been renamed (or does not exist)"
fi

# Count remaining bridge files
BRIDGE_COUNT=$(find "$REPO_ROOT/src/integration/legacy_bnetd/src" -name '*_bridge.cpp' 2>/dev/null | wc -l)
LINK_COUNT=$(find "$REPO_ROOT/src/integration/legacy_bnetd/src" -name '*_link.cpp' 2>/dev/null | wc -l)
echo "  INFO: $BRIDGE_COUNT bridge files remain in integration/legacy_bnetd/src/"
echo "  INFO: $LINK_COUNT link files remain in integration/legacy_bnetd/src/"

echo ""

# ---------------------------------------------------------------------------
# Step 4: Remove empty stub files left by lifecycle bridge collapse
# ---------------------------------------------------------------------------
echo "--- Step 4: Empty lifecycle bridge stub files ---"
STUB_FILES=(
    "$REPO_ROOT/src/integration/legacy_bnetd/src/bnetd_lifecycle_bridges_r246.cpp"
    "$REPO_ROOT/src/integration/legacy_bnetd/src/bnetd_lifecycle_bridges_r247.cpp"
)
for f in "${STUB_FILES[@]}"; do
    if [[ -f "$f" ]]; then
        echo "  STUB: $f"
        if [[ "$DRY_RUN" = false ]]; then
            rm -f "$f"
            echo "    DELETED: $f"
        fi
    fi
done

echo ""
if [[ "$DRY_RUN" = true ]]; then
    echo "DRY RUN complete. No changes made."
    echo "Run with --apply to perform the actual retirement steps."
    echo ""
    echo "For full retirement, complete Plan 06 phases 1-7 first:"
    echo "  plans/16-strangler-completion-detail.md"
else
    echo "Retirement steps applied."
    echo "Remember to:"
    echo "  1. Remove bnetd_lifecycle_bridges_r246.cpp and _r247.cpp from src/CMakeLists.txt"
    echo "     (already done if you ran the Plan 06 quick-wins task)"
    echo "  2. Run cmake --preset v3-dev && cmake --build --preset v3-dev"
    echo "  3. git add -A && git commit -m 'chore: retire lifecycle bridge stubs (Plan 06 quick wins)'"
fi
