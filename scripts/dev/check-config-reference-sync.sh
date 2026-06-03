#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# check-config-reference-sync.sh
#
# Verifies that docs/developer/config-reference.md is the up-to-date generation
# of the v3 configuration *schema* — i.e. it matches `gen-config-docs.sh`
# (which wraps `pvpgn_config_tool --print-schema`). Per plans/13, the reference
# doc is generated from the config schema in core/config, NOT hand-maintained
# and NOT compared against the operator template conf/bnetd.toml.in (that
# template is a richer superset: it also carries example / optional / D2 / icon
# sections that the typed bnetd schema does not expose).
#
# Strategy: regenerate to a temp file and diff against the committed doc.
#
# Exit codes
#   0  — in sync (committed doc == freshly generated)
#   1  — stale (regenerate with scripts/dev/gen-config-docs.sh)
#   2  — cannot verify (pvpgn_config_tool not built); pass its path or build it.
#
# Usage
#   scripts/dev/check-config-reference-sync.sh [TOOL_PATH]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DOC="${REPO_ROOT}/docs/developer/config-reference.md"
GEN="${REPO_ROOT}/scripts/dev/gen-config-docs.sh"

# Locate the built config tool: explicit arg, else any build/ tree.
TOOL="${1:-}"
if [[ -z "${TOOL}" ]]; then
    TOOL="$(find "${REPO_ROOT}/build" -name 'pvpgn_config_tool' -type f 2>/dev/null | head -1)"
fi

if [[ -z "${TOOL}" || ! -x "${TOOL}" ]]; then
    echo "config-reference-sync: pvpgn_config_tool not built — cannot verify."
    echo "  Build it (cmake --build <preset> --target pvpgn_config_tool) or pass its path."
    exit 2
fi

if [[ ! -f "${DOC}" ]]; then
    echo "config-reference-sync: ${DOC} not found" >&2
    exit 1
fi

TMP="$(mktemp)"
trap 'rm -f "${TMP}"' EXIT

bash "${GEN}" --tool "${TOOL}" --out "${TMP}" >/dev/null

if diff -q "${DOC}" "${TMP}" >/dev/null 2>&1; then
    echo "config-reference-sync: ${DOC} is up to date with --print-schema — OK"
    exit 0
fi

echo "config-reference-sync: ${DOC} is STALE relative to the config schema."
echo "  Regenerate it:  scripts/dev/gen-config-docs.sh --tool ${TOOL}"
echo "  ---- diff (committed vs generated) ----"
diff "${DOC}" "${TMP}" | head -40
exit 1
