#!/usr/bin/env sh
# Plan 10 step 2 — legacy/backend linkage ban for unit tests.
#
# A unit test (anything under tests/unit/) must not link a legacy target or a
# concrete DB-backend target: those drag in environment dependencies (a real
# DB, the legacy bnetd tree) and defeat the "fast, hermetic unit test" goal.
# In-memory fakes (infra_inmemory) are the supported seam.
#
# Banned target name fragments in `DEPS`/`target_link_libraries` lines:
#   bnetd_legacy, integration_legacy_* (the strangled legacy trees), and the
#   server-backed DB adapters mysql / postgresql (need a live DB server).
#
# SQLite is intentionally NOT banned: `:memory:` SQLite is hermetic (no server,
# no files) and is exactly what Plan 07's repository test matrix uses for its
# fast column — see tests/unit/infra/persistence/.
#
# Usage:  scripts/dev/check-test-legacy-linkage.sh [tests-root]
set -eu

ROOT="${1:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)/tests/unit}"
[ -d "$ROOT" ] || { echo "no tests/unit dir at $ROOT" >&2; exit 2; }

BANNED='bnetd_legacy|integration_legacy|_legacy_linked|pvpgn_infra_mysql|pvpgn_infra_postgresql|infra_mysql|infra_postgres'

violations=0
for cml in $(find "$ROOT" -name 'CMakeLists.txt'); do
    # Look at DEPS / target_link_libraries lines only.
    hits="$(grep -nE 'DEPS|target_link_libraries' "$cml" -A6 2>/dev/null \
            | grep -nE "$BANNED" || true)"
    if [ -n "$hits" ]; then
        echo "VIOLATION in ${cml#"$(dirname "$ROOT")"/}:"
        echo "$hits" | sed 's/^/    /'
        violations=$((violations + 1))
    fi
done

if [ "$violations" -gt 0 ]; then
    echo ""
    echo "Legacy-linkage ban: $violations test CMakeLists link a banned target." >&2
    exit 1
fi
echo "Legacy-linkage ban: no unit test links a legacy or DB-backend target."
