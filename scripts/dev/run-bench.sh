#!/usr/bin/env bash
# Plan 13 — benchmark runner.
#
# Builds and runs a benchmark suite, writing a bench-results.json stamped with
# the git rev + host. The bench targets are EXCLUDE_FROM_ALL, so this is the
# supported way to run them (they are never built by `make all` or ctest).
#
# Usage:  scripts/dev/run-bench.sh [micro]   (default: micro)
#         BUILD_DIR=build/v3-dev scripts/dev/run-bench.sh micro
set -eu

SUITE="${1:-micro}"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"

case "$SUITE" in
    micro) TARGET=bench_micro ;;
    *) echo "run-bench: unknown suite '$SUITE' (known: micro)" >&2; exit 2 ;;
esac

ncpu() { nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4; }

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    echo "run-bench: configuring $BUILD ..."
    cmake -S "$ROOT" -B "$BUILD" >/dev/null
fi

echo "run-bench: building $TARGET ..."
cmake --build "$BUILD" --target "$TARGET" -j"$(ncpu)" >/dev/null

BIN="$(find "$BUILD" -name "$TARGET" -type f -perm -u+x 2>/dev/null | head -1)"
[ -n "$BIN" ] || { echo "run-bench: built target $TARGET not found under $BUILD" >&2; exit 1; }

GITREV="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
HOST="$(uname -srm 2>/dev/null || echo unknown)"
OUT="$ROOT/bench-results.json"

"$BIN" --json "$OUT" --git-rev "$GITREV" --host "$HOST"
echo "run-bench: wrote $OUT (git $GITREV)"
