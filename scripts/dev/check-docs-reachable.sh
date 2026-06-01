#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# check-docs-reachable.sh — assert every .md in the mkdocs nav is reachable
# from docs/index.md within 3 clicks (hops).
#
# Usage
# -----
#   scripts/dev/check-docs-reachable.sh [--mkdocs <path>] [--docs <dir>] [--hops <n>]
#
# Options
#   --mkdocs <path>   Path to mkdocs.yml.  Default: mkdocs.yml
#   --docs   <dir>    Docs root directory.  Default: docs
#   --hops   <n>      Maximum hops from index.md.  Default: 3
#
# Exit codes
#   0  All nav pages reachable within the hop limit
#   1  One or more nav pages are NOT reachable
#
# Requires: bash, python3 (for YAML parsing), grep, sed

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

MKDOCS="${REPO_ROOT}/mkdocs.yml"
DOCS_DIR="${REPO_ROOT}/docs"
MAX_HOPS=3

# ── argument parsing ──────────────────────────────────────────────────────────

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mkdocs) MKDOCS="$2"; shift 2 ;;
        --docs)   DOCS_DIR="$2"; shift 2 ;;
        --hops)   MAX_HOPS="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,/^$/p' "$0" | grep '^#' | sed 's/^# \?//'
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# ── extract nav file paths from mkdocs.yml ────────────────────────────────────

NAV_FILES="$(python3 - "${MKDOCS}" <<'PYEOF'
import sys, re

path = sys.argv[1]
with open(path) as f:
    content = f.read()

# Extract all .md paths from nav entries (value after ': ')
# Handles both "Title: path.md" and "- Title: path.md" forms
for m in re.finditer(r':\s+([^\s#\'"]+\.md)', content):
    print(m.group(1))
PYEOF
)"

if [[ -z "${NAV_FILES}" ]]; then
    echo "ERROR: No .md files found in nav section of ${MKDOCS}" >&2
    exit 1
fi

echo "Nav pages found: $(echo "${NAV_FILES}" | wc -l | tr -d ' ')"

# ── extract links from a markdown file ───────────────────────────────────────

extract_links() {
    local file="$1"
    local file_dir
    file_dir="$(dirname "${file}")"

    # Extract markdown links: [text](path) — only relative .md links
    grep -oE '\[([^]]*)\]\(([^)]+)\)' "${file}" 2>/dev/null \
        | grep -oE '\(([^)]+)\)' \
        | tr -d '()' \
        | grep '\.md' \
        | grep -v '^http' \
        | sed 's/#.*//' \
        | while read -r link; do
            # Resolve relative path
            if [[ "${link}" == /* ]]; then
                echo "${DOCS_DIR}${link}"
            else
                realpath -m "${file_dir}/${link}" 2>/dev/null || echo "${file_dir}/${link}"
            fi
        done
}

# ── BFS reachability from index.md ───────────────────────────────────────────

INDEX="${DOCS_DIR}/index.md"

if [[ ! -f "${INDEX}" ]]; then
    echo "ERROR: ${INDEX} not found" >&2
    exit 1
fi

# visited: set of absolute paths already seen
declare -A visited
# frontier: files to process at current hop level
frontier=("${INDEX}")
visited["${INDEX}"]=1

hop=0
while [[ ${hop} -lt ${MAX_HOPS} && ${#frontier[@]} -gt 0 ]]; do
    hop=$(( hop + 1 ))
    next_frontier=()
    for f in "${frontier[@]}"; do
        if [[ ! -f "${f}" ]]; then
            continue
        fi
        while IFS= read -r linked; do
            if [[ -n "${linked}" && -z "${visited[${linked}]+x}" ]]; then
                visited["${linked}"]=1
                next_frontier+=("${linked}")
            fi
        done < <(extract_links "${f}")
    done
    frontier=("${next_frontier[@]+"${next_frontier[@]}"}")
done

# ── check each nav page ───────────────────────────────────────────────────────

unreachable=()

while IFS= read -r nav_path; do
    [[ -z "${nav_path}" ]] && continue
    abs_path="${DOCS_DIR}/${nav_path}"
    abs_path="$(realpath -m "${abs_path}" 2>/dev/null || echo "${abs_path}")"

    if [[ -z "${visited[${abs_path}]+x}" ]]; then
        unreachable+=("${nav_path}")
    fi
done <<< "${NAV_FILES}"

# ── report ────────────────────────────────────────────────────────────────────

if [[ ${#unreachable[@]} -eq 0 ]]; then
    echo "✅  All nav pages reachable from docs/index.md within ${MAX_HOPS} hops."
    exit 0
else
    echo "❌  The following nav pages are NOT reachable from docs/index.md within ${MAX_HOPS} hops:"
    for p in "${unreachable[@]}"; do
        echo "    - ${p}"
    done
    echo ""
    echo "Fix: add links to these pages in docs/index.md or in pages reachable from it."
    exit 1
fi
