#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# retire-legacy.sh — Dry-run script showing what would be deleted/moved
# in the final legacy retirement step (R354).
#
# Usage:
#   ./scripts/dev/retire-legacy.sh          # dry run (default)
#   ./scripts/dev/retire-legacy.sh --apply  # actually perform the changes
#
# WARNING: --apply is IRREVERSIBLE. Commit everything first.
set -euo pipefail

DRY_RUN=true
if [[ "${1:-}" == "--apply" ]]; then
    DRY_RUN=false
fi

LEGACY_DIRS=(
    "src/bnetd"
    "src/d2cs"
    "src/d2dbs"
    "src/compat"
)

V3_SOURCE="src/v3"
V3_DEST="src"

echo "=== PvPGN Legacy Retirement Script ==="
echo "Mode: $([ "$DRY_RUN" = true ] && echo 'DRY RUN' || echo 'APPLY')"
echo ""

echo "--- Step 1: Files to DELETE (legacy sources) ---"
for dir in "${LEGACY_DIRS[@]}"; do
    if [[ -d "$dir" ]]; then
        echo "  DELETE: $dir/"
        if [[ "$DRY_RUN" = false ]]; then
            rm -rf "$dir"
        fi
    fi
done

echo ""
echo "--- Step 2: Move src/v3/* → src/ ---"
if [[ -d "$V3_SOURCE" ]]; then
    for item in "$V3_SOURCE"/*/; do
        name=$(basename "$item")
        echo "  MOVE: $V3_SOURCE/$name → $V3_DEST/$name"
        if [[ "$DRY_RUN" = false ]]; then
            mv "$item" "$V3_DEST/$name"
        fi
    done
    if [[ "$DRY_RUN" = false ]]; then
        rmdir "$V3_SOURCE" 2>/dev/null || true
    fi
fi

echo ""
if [[ "$DRY_RUN" = true ]]; then
    echo "DRY RUN complete. No changes made."
    echo "Run with --apply to perform the actual retirement."
else
    echo "Legacy retirement complete."
    echo "Remember to:"
    echo "  1. Update CMakeLists.txt paths (src/v3/ → src/)"
    echo "  2. Update #include paths in source files"
    echo "  3. Run cmake --preset v3-dev && cmake --build --preset v3-dev"
    echo "  4. git add -A && git commit -m 'chore: retire legacy sources (R354)'"
fi
