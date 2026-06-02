#!/usr/bin/env bash
# Plan 15 — CHANGELOG discipline gate (Keep a Changelog).
#
# Enforces that CHANGELOG.md follows https://keepachangelog.com :
#   1. references Keep a Changelog and Semantic Versioning;
#   2. has an `## [Unreleased]` section (so merged changes have a home);
#   3. uses only the canonical change headings (Added / Changed / Deprecated /
#      Removed / Fixed / Security) at the `### ` level;
#   4. every released section is `## [x.y.z] - YYYY-MM-DD`.
#
# Usage: scripts/dev/check-changelog.sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
CL="$ROOT/CHANGELOG.md"

[ -f "$CL" ] || { echo "changelog: missing $CL" >&2; exit 2; }

fail=0
err() { echo "changelog: FAIL — $*" >&2; fail=1; }

grep -qi "keep a changelog" "$CL" || err "missing a Keep a Changelog reference"
grep -qi "semantic versioning\|semver" "$CL" || err "missing a Semantic Versioning reference"
grep -qE "^## \[Unreleased\]" "$CL" || err "missing an '## [Unreleased]' section"

# Canonical Keep a Changelog headings.
allowed='Added Changed Deprecated Removed Fixed Security'
while IFS= read -r h; do
    name="$(printf '%s' "$h" | sed -E 's/^###[[:space:]]+//')"
    case " $allowed " in
        *" $name "*) : ;;
        *) err "non-standard change heading '### $name' (allowed: $allowed)" ;;
    esac
done < <(grep -E "^### " "$CL" || true)

# Released sections must look like '## [x.y.z] - YYYY-MM-DD' (any dash glyph).
while IFS= read -r line; do
    case "$line" in
        '## [Unreleased]'*) : ;;
        '## ['*)
            printf '%s\n' "$line" \
              | grep -qE '^## \[[0-9]+\.[0-9]+\.[0-9]+\] .+ [0-9]{4}-[0-9]{2}-[0-9]{2}' \
              || err "malformed release heading: $line"
            ;;
    esac
done < <(grep -E "^## \[" "$CL" || true)

[ "$fail" -eq 0 ] && echo "changelog: $CL follows Keep a Changelog — OK"
exit "$fail"
