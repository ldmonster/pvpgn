#!/usr/bin/env bash
# Plan 10 — coverage gate (standard per-file union line coverage).
#
# Computes gcov line coverage for the v3 `domain/` and `application/` layers as
# the UNION of covered line numbers per source file across all translation
# units — the standard definition used by lcov / gcovr / llvm-cov.
#
# Why not the old method: the previous version summed `gcov -n` "Lines
# executed:P% of N" across every object, which counts a header once per
# *including* TU. A value object like `domain/shared/user_name.hpp`, pulled into
# hundreds of TUs and only partially exercised in most of them, was therefore
# counted hundreds of times — dragging the ratio down by ~25 points and, worse,
# inflating the denominator every time a new test TU was added (so writing more
# tests barely moved, or could even lower, the number). Measured the two side by
# side on 2026-06-05: old method 69.3%, this per-file-union method 96.6% over the
# same .gcda. See docs/refactoring/progress.md (coverage-fleet arc).
#
# Usage:   scripts/dev/check-coverage.sh [build-dir] [floor-percent]
# Env:     COVERAGE_FLOOR   floor as a percentage (default 90); arg overrides.
#          COVERAGE_PATHS   space-separated path fragments to include
#                           (default: "/src/domain/ /src/application/").
#
# Requires `gcov` + `python3` (python3 is already required by the e2e gates).
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build/v3-coverage}"
FLOOR="${2:-${COVERAGE_FLOOR:-90}}"
PATHS="${COVERAGE_PATHS:-/src/domain/ /src/application/}"

if [ ! -d "$BUILD" ]; then
    echo "coverage: build dir not found: $BUILD" >&2
    echo "  configure+run a coverage build first, e.g.:" >&2
    echo "    cmake --preset v3-coverage && cmake --build --preset v3-coverage" >&2
    echo "    ctest --preset v3-coverage" >&2
    exit 2
fi
# Absolutize: each object is processed from its own temp CWD below, so a
# relative .gcno path would no longer resolve.
BUILD="$(CDPATH= cd -- "$BUILD" && pwd)"

command -v gcov    >/dev/null 2>&1 || { echo "coverage: gcov not found"    >&2; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo "coverage: python3 not found" >&2; exit 2; }

# Emit gcov intermediate JSON (per-line hit counts) for every instrumented
# object. Each object is processed in its own temp subdir because `gcov -i`
# writes "<source-basename>.gcov.json.gz" into the CWD, so two objects sharing a
# basename (common for per-state split TUs) would otherwise clobber each other.
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

i=0
while IFS= read -r -d '' gcno; do
    d="$WORK/$i"; mkdir -p "$d"
    ( cd "$d" && gcov -i "$gcno" >/dev/null 2>&1 || true )
    i=$((i + 1))
done < <(find "$BUILD" -name '*.gcno' -print0)

# Union covered line numbers per source file across all JSON, restricted to the
# selected path fragments, then compute covered/total.
summary="$(
  python3 - "$WORK" "$PATHS" <<'PY'
import sys, os, glob, gzip, json, collections

work  = sys.argv[1]
paths = sys.argv[2].split()

covered = collections.defaultdict(set)   # file -> {covered line numbers}
total   = collections.defaultdict(set)   # file -> {instrumented line numbers}

for fn in glob.glob(os.path.join(work, '*', '*.gcov.json.gz')):
    try:
        with gzip.open(fn) as fh:
            data = json.load(fh)
    except Exception:
        continue
    for f in data.get('files', []):
        name = f.get('file', '')
        if not any(p in name for p in paths):
            continue
        for ln in f.get('lines', []):
            n = ln.get('line_number')
            if n is None:
                continue
            total[name].add(n)
            if ln.get('count', 0) > 0:
                covered[name].add(n)

T = sum(len(v) for v in total.values())
C = sum(len(covered[k]) for k in total)
pct = (100.0 * C / T) if T else 0.0
print(f"{C} {T} {pct:.2f}")
PY
)"

covered="$(echo "$summary" | awk '{print $1}')"
total="$(echo "$summary"   | awk '{print $2}')"
pct="$(echo "$summary"     | awk '{print $3}')"

echo "Coverage (domain + application): ${pct}% (~${covered}/${total} lines, per-file union)"
echo "Floor: ${FLOOR}%"

if [ "${total:-0}" -eq 0 ]; then
    echo "coverage: no instrumented domain/application lines found in $BUILD" >&2
    echo "  did ctest run against the coverage build (to emit .gcda)?" >&2
    exit 2
fi

# Compare with awk (floats); exit non-zero if below the floor.
if awk -v p="$pct" -v f="$FLOOR" 'BEGIN { exit !(p + 0 < f + 0) }'; then
    echo "FAIL: coverage ${pct}% is below the ${FLOOR}% floor." >&2
    exit 1
fi
echo "PASS: coverage ${pct}% meets the ${FLOOR}% floor."
