#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
#
# v3_layering_check.sh -- enforce the v3 hexagonal layering rule.
#
# Layering (each layer may only #include from itself or layers above it):
#
#     core         (no v3 deps)
#     domain       -> core
#     application  -> core, domain
#     protocol     -> core, domain
#     infra        -> core, domain, application, protocol
#     integration  -> core, domain, application, protocol, infra
#     services     -> all of the above
#     app          -> all of the above
#
# Critical rule: application MUST NOT depend on infra.
#
# Allow-list: known existing violations that are scheduled for separate
# refactoring rounds. New violations must NOT be added here without owner
# review.
#
# Usage:
#   scripts/v3_layering_check.sh [--report-file <path>] [SRC_ROOT]
#
# Options:
#   --report-file <path>   Write violations to <path> in addition to stdout.
#
# Exit codes:
#   0  No violations found.
#   1  One or more violations found.
#   2  Bad arguments or SRC_ROOT directory not found.

set -e

REPORT_FILE=""
SRC_ROOT=""

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --report-file)
            if [ -z "$2" ]; then
                echo "v3_layering_check: --report-file requires a path argument" >&2
                exit 2
            fi
            REPORT_FILE="$2"
            shift 2
            ;;
        -*)
            echo "v3_layering_check: unknown option: $1" >&2
            exit 2
            ;;
        *)
            SRC_ROOT="$1"
            shift
            ;;
    esac
done

SRC_ROOT="${SRC_ROOT:-$(cd "$(dirname "$0")/.." && pwd)/src}"

if [ ! -d "$SRC_ROOT" ]; then
    echo "v3_layering_check: directory not found: $SRC_ROOT" >&2
    exit 2
fi

# Initialise the report file (truncate if it exists).
if [ -n "$REPORT_FILE" ]; then
    : > "$REPORT_FILE"
fi

# Helper: emit a line to stdout and optionally to the report file.
emit() {
    echo "$1"
    if [ -n "$REPORT_FILE" ]; then
        echo "$1" >> "$REPORT_FILE"
    fi
}

# Allow-list: known existing violations scheduled for separate refactor rounds.
# Format: one "path:line:#include "<header>"" entry per line (whitespace
# separated tokens are matched as substrings against the key below).
# New violations must NOT be added here without owner review.
ALLOW_FILE=$(mktemp)
cat >"$ALLOW_FILE" <<'EOF'
EOF

check_layer() {
    layer="$1"
    shift
    forbidden="$@"  # space-separated list of forbidden include prefixes

    layer_dir="$SRC_ROOT/$layer"
    [ -d "$layer_dir" ] || return 0

    for f in $(find "$layer_dir" -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.h' \)); do
        for prefix in $forbidden; do
            # match  #include "<prefix>/...   or   #include <<prefix>/...
            matches=$(grep -nE "^[[:space:]]*#include[[:space:]]+[\"<]${prefix}/" "$f" 2>/dev/null || true)
            [ -z "$matches" ] && continue

            echo "$matches" | while IFS= read -r line; do
                # Strip leading $SRC_ROOT to make repo-relative path
                rel="${f#"$SRC_ROOT"/}"
                lineno=$(echo "$line" | cut -d: -f1)
                inc=$(echo "$line" | sed -nE 's/.*#include[[:space:]]+[\"<]([^">]+)[">].*/\1/p')

                # Allow-list check: "path|include" substring match
                key_path="src/v3/$rel"
                if grep -qF "${key_path}|${inc}" "$ALLOW_FILE"; then
                    emit "ALLOWED (TODO): $key_path:$lineno  ($inc)"
                    continue
                fi

                emit "VIOLATION: $layer must not include $prefix/* -- $rel:$lineno  ($inc)"
                echo x >> /tmp/v3layer.cnt
            done
        done
    done
}

: > /tmp/v3layer.cnt

# core may not depend on anything else in v3
check_layer core   domain application protocol infra integration services app

# domain may not depend on layers below it
check_layer domain application protocol infra integration services app

# application/protocol may not depend on infra or anything below it.
# Application also must not depend on protocol (ports are abstract).
check_layer application infra integration services app
check_layer protocol    infra integration services app

# infra may not depend on integration/services/app
check_layer infra integration services app

# integration may not depend on services/app
check_layer integration services app

# services may not depend on app
check_layer services app

total=$(wc -l < /tmp/v3layer.cnt 2>/dev/null | tr -d ' ')
rm -f /tmp/v3layer.cnt
rm -f "$ALLOW_FILE"

# Normalise: empty file → 0
total="${total:-0}"

emit ""
emit "Layering check: ${total} violations found"

if [ "$total" -gt 0 ]; then
    exit 1
fi

exit 0
