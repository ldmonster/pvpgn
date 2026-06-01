#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# check-scripts-orphans.sh — report any file under scripts/ that has zero
# references in Dockerfiles, CI workflows, docs, other scripts, or README.md.
#
# Usage
# -----
#   scripts/dev/check-scripts-orphans.sh [--scripts <dir>] [--root <dir>]
#
# Options
#   --scripts <dir>   Scripts directory to audit.  Default: scripts
#   --root    <dir>   Repository root.  Default: auto-detected
#
# Exit codes
#   0  No orphan scripts found
#   1  One or more orphan scripts found
#
# Requires: bash, grep, find

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SCRIPTS_DIR="${REPO_ROOT}/scripts"

# ── argument parsing ──────────────────────────────────────────────────────────

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scripts) SCRIPTS_DIR="$2"; shift 2 ;;
        --root)    REPO_ROOT="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ── build the list of files to search for references ─────────────────────────

# Reference search targets:
#   1. Dockerfile* in repo root
#   2. .github/workflows/*.yml
#   3. docs/**/*.md
#   4. Other script files under scripts/
#   5. README.md

mapfile -t SEARCH_FILES < <(
    find "${REPO_ROOT}" -maxdepth 1 -name 'Dockerfile*' -type f
    find "${REPO_ROOT}/.github/workflows" -name '*.yml' -type f 2>/dev/null || true
    find "${REPO_ROOT}/docs" -name '*.md' -type f 2>/dev/null || true
    find "${REPO_ROOT}/scripts" -type f 2>/dev/null || true
    [[ -f "${REPO_ROOT}/README.md" ]] && echo "${REPO_ROOT}/README.md" || true
)

if [[ ${#SEARCH_FILES[@]} -eq 0 ]]; then
    echo "WARNING: No reference files found to search in." >&2
fi

# ── enumerate all script files ────────────────────────────────────────────────

mapfile -t ALL_SCRIPTS < <(
    find "${SCRIPTS_DIR}" -type f | sort
)

echo "Auditing ${#ALL_SCRIPTS[@]} script files for references..."

# ── check each script ─────────────────────────────────────────────────────────

orphans=()

for script in "${ALL_SCRIPTS[@]}"; do
    # Get just the filename and the path relative to repo root
    script_name="$(basename "${script}")"
    script_rel="$(realpath --relative-to="${REPO_ROOT}" "${script}")"

    found=0

    for ref_file in "${SEARCH_FILES[@]}"; do
        # Skip self-reference
        [[ "${ref_file}" == "${script}" ]] && continue
        # Skip non-existent files
        [[ ! -f "${ref_file}" ]] && continue

        # Search for the script name or relative path in the reference file
        if grep -qF "${script_name}" "${ref_file}" 2>/dev/null || \
           grep -qF "${script_rel}" "${ref_file}" 2>/dev/null; then
            found=1
            break
        fi
    done

    if [[ ${found} -eq 0 ]]; then
        orphans+=("${script_rel}")
    fi
done

# ── report ────────────────────────────────────────────────────────────────────

if [[ ${#orphans[@]} -eq 0 ]]; then
    echo "✅  No orphan scripts found — every script is referenced."
    exit 0
else
    echo "❌  The following scripts have zero references in Dockerfiles, CI workflows, docs, other scripts, or README.md:"
    for s in "${orphans[@]}"; do
        echo "    - ${s}"
    done
    echo ""
    echo "Fix: reference each orphan script in a Dockerfile, .github/workflows/*.yml, docs/**/*.md, README.md, or another script."
    exit 1
fi
