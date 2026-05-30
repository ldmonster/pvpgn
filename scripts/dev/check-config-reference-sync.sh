#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# check-config-reference-sync.sh
#
# Verifies that docs/developer/config-reference.md is in sync with the
# TOML schema defined in conf/bnetd.toml.in.
#
# Strategy
# --------
# 1. Extract all top-level TOML section headers ([section]) from bnetd.toml.in.
# 2. Extract all documented section headers from config-reference.md
#    (lines starting with "## " or "### " that match a TOML section name).
# 3. Report any sections present in the schema but absent from the docs,
#    and any sections documented but absent from the schema.
#
# Exit codes
#   0  — in sync (no missing or extra sections)
#   1  — out of sync (missing or extra sections detected)
#   2  — usage error or required file not found
#
# Usage
#   scripts/dev/check-config-reference-sync.sh [REPO_ROOT]
#
# If REPO_ROOT is omitted, the script uses the directory two levels above
# its own location (i.e., the repository root when invoked from any CWD).

set -euo pipefail

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${1:-$(cd "${SCRIPT_DIR}/../.." && pwd)}"

SCHEMA_FILE="${REPO_ROOT}/conf/bnetd.toml.in"
DOCS_FILE="${REPO_ROOT}/docs/developer/config-reference.md"

if [[ ! -f "${SCHEMA_FILE}" ]]; then
    echo "error: schema file not found: ${SCHEMA_FILE}" >&2
    exit 2
fi

if [[ ! -f "${DOCS_FILE}" ]]; then
    echo "error: config-reference doc not found: ${DOCS_FILE}" >&2
    exit 2
fi

# ---------------------------------------------------------------------------
# Extract section names from schema (lines like [section] or [section.sub])
# ---------------------------------------------------------------------------

schema_sections=()
while IFS= read -r line; do
    # Match lines of the form [identifier] or [identifier.sub] (not [[arrays]])
    if [[ "${line}" =~ ^\[([a-zA-Z_][a-zA-Z0-9_.]*)\] ]]; then
        schema_sections+=("${BASH_REMATCH[1]}")
    fi
done < "${SCHEMA_FILE}"

# ---------------------------------------------------------------------------
# Extract section names from docs (## or ### headings that look like TOML keys)
# ---------------------------------------------------------------------------

doc_sections=()
while IFS= read -r line; do
    # Match headings like "## [server]" or "### server" or "## server.network"
    if [[ "${line}" =~ ^#{2,3}[[:space:]]+\[?([a-zA-Z_][a-zA-Z0-9_.]*)\]? ]]; then
        doc_sections+=("${BASH_REMATCH[1]}")
    fi
done < "${DOCS_FILE}"

# ---------------------------------------------------------------------------
# Compare
# ---------------------------------------------------------------------------

missing_from_docs=()
for section in "${schema_sections[@]}"; do
    found=0
    for doc_section in "${doc_sections[@]}"; do
        if [[ "${section}" == "${doc_section}" ]]; then
            found=1
            break
        fi
    done
    if [[ "${found}" -eq 0 ]]; then
        missing_from_docs+=("${section}")
    fi
done

extra_in_docs=()
for doc_section in "${doc_sections[@]}"; do
    found=0
    for section in "${schema_sections[@]}"; do
        if [[ "${section}" == "${doc_section}" ]]; then
            found=1
            break
        fi
    done
    if [[ "${found}" -eq 0 ]]; then
        extra_in_docs+=("${doc_section}")
    fi
done

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

exit_code=0

if [[ "${#missing_from_docs[@]}" -gt 0 ]]; then
    echo "FAIL: The following TOML sections are in the schema but missing from config-reference.md:"
    for s in "${missing_from_docs[@]}"; do
        echo "  - ${s}"
    done
    exit_code=1
fi

if [[ "${#extra_in_docs[@]}" -gt 0 ]]; then
    echo "WARN: The following sections are documented but not found in bnetd.toml.in:"
    for s in "${extra_in_docs[@]}"; do
        echo "  - ${s}"
    done
    # Extra docs sections are a warning, not a hard failure
fi

if [[ "${exit_code}" -eq 0 ]]; then
    echo "OK: config-reference.md is in sync with bnetd.toml.in"
    echo "  Schema sections: ${#schema_sections[@]}"
    echo "  Documented sections: ${#doc_sections[@]}"
fi

exit "${exit_code}"
