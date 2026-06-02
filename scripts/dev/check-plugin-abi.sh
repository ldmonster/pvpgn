#!/usr/bin/env bash
# Plan 12 — plugin ABI semver gate.
#
# The public plugin ABI header `include/pvpgn/plugin/abi.h` is a frozen contract
# for ABI version `PVPGN_PLUGIN_ABI_VERSION`. This gate diffs it against the
# committed golden snapshot and FAILS on any change, so a reviewer is forced to
# classify it:
#
#   * Non-breaking (only APPENDED struct fields / new macros / new optional
#     exports / comments): refresh the golden with `--update` in the same PR.
#   * BREAKING (changed/removed/reordered a field, changed a signature or a
#     capability bit value): bump to a NEW header `pvpgn/plugin/abi_v2.h`,
#     keep `abi.h` (v1) for the deprecation window, and snapshot the new one.
#
# Usage:   scripts/dev/check-plugin-abi.sh [--update]
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
HEADER="$ROOT/include/pvpgn/plugin/abi.h"
GOLDEN="$ROOT/tests/abi/pvpgn_plugin_abi_v1.h.golden"

[ -f "$HEADER" ] || { echo "plugin-abi: missing $HEADER" >&2; exit 2; }

if [ "${1:-}" = "--update" ]; then
    cp "$HEADER" "$GOLDEN"
    echo "plugin-abi: golden snapshot refreshed from $HEADER"
    echo "  (confirm the change is NON-breaking, or that you also bumped the ABI version)"
    exit 0
fi

[ -f "$GOLDEN" ] || { echo "plugin-abi: missing golden $GOLDEN (run --update once)" >&2; exit 2; }

if diff -u "$GOLDEN" "$HEADER" >/tmp/plugin_abi_diff.$$ 2>/dev/null; then
    rm -f /tmp/plugin_abi_diff.$$
    echo "plugin-abi: $HEADER matches the v1 golden — OK"
    exit 0
fi

echo "plugin-abi: the public ABI header changed vs the committed golden:" >&2
echo "" >&2
cat /tmp/plugin_abi_diff.$$ >&2
rm -f /tmp/plugin_abi_diff.$$
echo "" >&2
echo "If this change is NON-breaking (appended fields / new macros / comments)," >&2
echo "  run: scripts/dev/check-plugin-abi.sh --update  and commit the golden." >&2
echo "If it is BREAKING, add pvpgn/plugin/abi_v2.h, keep v1 for the deprecation" >&2
echo "  window, and snapshot the new header instead." >&2
exit 1
