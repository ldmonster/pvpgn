#!/usr/bin/env sh
# Plan 10 step 1 — unit-test pairing audit.
#
# Every production translation unit under src/domain/ and src/application/
# should have a paired unit test somewhere under tests/unit/ named
# "<stem>_test.cpp". This script lists the unpaired ones and exits non-zero
# if any are found, so CI can fail on a regression.
#
# Usage:  scripts/dev/check-unit-pairing.sh [src-root]
# Env:    PAIRING_ALLOW  newline/space list of stems exempt from the rule.
set -eu

ROOT="${1:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)}"
SRC="$ROOT/src"
TESTS="$ROOT/tests/unit"

[ -d "$SRC" ] || { echo "no src dir at $SRC" >&2; exit 2; }

# Build a lookup of every existing test stem ("<stem>_test").
test_stems="$(find "$TESTS" -name '*_test.cpp' 2>/dev/null \
    | sed -E 's#.*/##; s#\.cpp$##' | sort -u)"

missing=0
for layer in domain application; do
    [ -d "$SRC/$layer" ] || continue
    # Production .cpp files under src/<layer>/.../src/*.cpp
    find "$SRC/$layer" -name '*.cpp' ! -name '*_test.cpp' 2>/dev/null \
    | while IFS= read -r f; do
        stem="$(basename "$f" .cpp)"
        # main.cpp / composition roots are not unit-tested in isolation.
        case "$stem" in main|*_main) continue;; esac
        case " ${PAIRING_ALLOW:-} " in *" $stem "*) continue;; esac
        if ! printf '%s\n' "$test_stems" | grep -qx "${stem}_test"; then
            rel="${f#"$ROOT"/}"
            echo "UNPAIRED: $rel  (expected a tests/unit/**/${stem}_test.cpp)"
        fi
    done
done | sort -u | tee /tmp/pairing.$$ || true

missing="$(wc -l < /tmp/pairing.$$ 2>/dev/null || echo 0)"
rm -f /tmp/pairing.$$
if [ "${missing:-0}" -gt 0 ]; then
    echo ""
    echo "Pairing audit: $missing unpaired production TU(s)." >&2
    exit 1
fi
echo "Pairing audit: every domain/application TU has a paired unit test."
