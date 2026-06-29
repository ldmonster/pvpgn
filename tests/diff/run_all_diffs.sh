#!/usr/bin/env bash
# Robust sequential runner for every tests/diff/diff_*.py.
#
# Why the naive `for f in ...; do timeout 150 python3 $f; done` got "stuck":
#   1. When `timeout` SIGTERMs a hung test, the test's CHILD servers (the
#      original bnetd/d2cs/d2dbs + the v3 binaries, launched via subprocess)
#      can be ORPHANED — they keep holding their TCP ports, so the next test
#      that needs those ports hangs on startup, and the stall cascades.
#   2. If the runner process itself dies, the run freezes at whatever count it
#      reached with no way to tell.
#
# Fixes here:
#   - `timeout --kill-after=15 --signal=TERM <T>`: if a test ignores SIGTERM it
#     is hard-SIGKILLed 15s later, so no test can wedge the loop.
#   - After every test, reap any leftover diff-test servers. They always run
#     from /tmp/pvpgn-* temp homes, so matching that path kills only orphaned
#     test servers (never this script: the running shell's argv is the script
#     PATH, not the pattern — avoiding the self-match pkill foot-gun).
#   - Resumable: a result already recorded PASS is skipped, so a re-launch after
#     a crash continues where it left off.
#   - A heartbeat file shows it is alive and which test is in flight.
#
# Usage:  tests/diff/run_all_diffs.sh [results-file] [per-test-timeout-secs]
set -u
cd "$(dirname "$0")/../.."

OUT="${1:-/tmp/pvpgn_diff_results.txt}"
TIMEOUT="${2:-150}"
HEARTBEAT="${OUT}.heartbeat"
V3BN="build/v3-dev/src/app/bnetd/bnetd"
touch "$OUT"

reap_orphans() {
    # Diff-test servers live under /tmp/pvpgn-* temp homes; reap any stragglers.
    pkill -9 -f '/tmp/pvpgn-' 2>/dev/null || true
}

total=$(ls tests/diff/diff_*.py | wc -l)
i=0
for f in tests/diff/diff_*.py; do
    i=$((i+1))
    name=$(basename "$f")
    if grep -qx "PASS $name" "$OUT" 2>/dev/null; then
        continue
    fi
    # drop any stale (e.g. crashed-mid-test) record for this test before re-running
    grep -v " $name\$" "$OUT" > "$OUT.tmp" 2>/dev/null && mv "$OUT.tmp" "$OUT"

    # diff_chat / diff_ols_login take no --v3-bnetd default; pass it explicitly.
    extra=""
    case "$name" in diff_chat.py|diff_ols_login.py) extra="--v3-bnetd $V3BN" ;; esac

    echo "[$i/$total] running $name (pid $$) at $(date +%H:%M:%S)" > "$HEARTBEAT"
    if timeout --kill-after=15 --signal=TERM "$TIMEOUT" python3 "$f" $extra >/dev/null 2>&1; then
        echo "PASS $name" >> "$OUT"
    else
        echo "FAIL $name (rc=$?)" >> "$OUT"
    fi
    reap_orphans      # prevent orphaned servers from wedging the next test
done

echo "=== SUMMARY: pass=$(grep -c '^PASS' "$OUT") fail=$(grep -c '^FAIL' "$OUT") of $total ===" | tee -a "$OUT"
grep '^FAIL' "$OUT" || true
rm -f "$HEARTBEAT"
