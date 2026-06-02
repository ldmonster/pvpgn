#!/usr/bin/env bash
# Plan 10 — coverage gate.
#
# Aggregates gcov line coverage for the v3 `domain/` and `application/` layers
# from a coverage build (configured with `--coverage`, e.g. the `v3-coverage`
# CMake preset after `ctest` has produced .gcda files) and fails if it falls
# below a floor.
#
# Usage:   scripts/dev/check-coverage.sh [build-dir] [floor-percent]
# Env:     COVERAGE_FLOOR   floor as a percentage (default 70); arg overrides.
#          COVERAGE_PATHS   space-separated path fragments to include
#                           (default: "/src/domain/ /src/application/").
#
# Requires only `gcov` (no gcovr/lcov), so it runs anywhere the toolchain does.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD="${1:-$ROOT/build/v3-coverage}"
FLOOR="${2:-${COVERAGE_FLOOR:-70}}"
PATHS="${COVERAGE_PATHS:-/src/domain/ /src/application/}"

if [ ! -d "$BUILD" ]; then
    echo "coverage: build dir not found: $BUILD" >&2
    echo "  configure+run a coverage build first, e.g.:" >&2
    echo "    cmake --preset v3-coverage && cmake --build --preset v3-coverage" >&2
    echo "    ctest --preset v3-coverage" >&2
    exit 2
fi

command -v gcov >/dev/null 2>&1 || { echo "coverage: gcov not found" >&2; exit 2; }

# Run gcov over every instrumented object and aggregate the per-file
# "Lines executed:P% of N" lines, weighting each file's percentage by N so the
# result is a true line-coverage ratio over the selected layers.
#
# awk accumulates sum(P*N) and sum(N) for files whose path matches one of the
# include fragments; the gate compares sum(P*N)/sum(N) against the floor.
summary="$(
  find "$BUILD" -name '*.gcno' -print0 \
  | while IFS= read -r -d '' gcno; do
        # gcov resolves the matching .gcda next to the .gcno.
        gcov -n -b "$gcno" 2>/dev/null || true
    done \
  | awk -v paths="$PATHS" '
      BEGIN { n = split(paths, P, " ") }
      /^File / {
          file = $0
          sub(/^File .\x27?/, "", file); sub(/\x27?.?$/, "", file)
          want = 0
          for (i = 1; i <= n; i++) if (P[i] != "" && index(file, P[i])) want = 1
          next
      }
      /^Lines executed:/ {
          # gcov -b prints "Lines executed" twice per file; count only the
          # first by clearing `want` once this file has been accumulated.
          if (!want) next
          want = 0
          # Format: Lines executed:PP.PP% of NNN
          split($0, a, ":"); split(a[2], b, "%")
          pct = b[1] + 0
          split($0, c, "of "); lines = c[2] + 0
          if (lines > 0) { wsum += pct * lines; tot += lines }
      }
      END {
          if (tot == 0) { print "0 0 0.00"; exit }
          printf "%d %d %.2f\n", wsum/100, tot, wsum/tot
      }'
)"

covered="$(echo "$summary" | awk '{print $1}')"
total="$(echo "$summary" | awk '{print $2}')"
pct="$(echo "$summary" | awk '{print $3}')"

echo "Coverage (domain + application): ${pct}% (~${covered}/${total} lines)"
echo "Floor: ${FLOOR}%"

if [ "$total" -eq 0 ]; then
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
