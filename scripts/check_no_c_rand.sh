#!/usr/bin/env bash
# check_no_c_rand.sh — forbid the C `rand()`/`srand()` PRNG in the v3 tree.
#
# Per plans/08-cross-cutting.md / Milestone 4, randomness is obtained through the
# RNG port (`domain/shared/ports/random_source.hpp`, `core::crypto::secure_random`,
# `infra/.../in_memory_random_source.hpp`) — never the non-deterministic,
# non-cryptographic C `std::rand`/`rand`/`srand`. The proper `<random>` engines
# (`std::random_device`, `std::mt19937`, `std::uniform_*`) are NOT forbidden.
#
# The legacy `src/common` tree (pre-v3 C code) is out of scope.
#
# Exit 0 = GREEN, Exit 1 = RED. No allow-list.

set -euo pipefail

ROOT="${1:-src}"
VIOLATIONS=0

# v3 layers only; skip the legacy src/common.
DIRS=()
for d in domain application core protocol services app infra runtime scripting integration; do
    [ -d "$ROOT/$d" ] && DIRS+=("$ROOT/$d")
done

# Match `std::rand(`, bare `rand(` (not preceded by an identifier char or ::),
# and `srand(`. Excludes `random_device`, member `.rand(`, `brand(`, etc.
PATTERN='(\bstd::rand[[:space:]]*\()|(\bsrand[[:space:]]*\()|((^|[^_a-zA-Z0-9:.])rand[[:space:]]*\()'

for dir in "${DIRS[@]}"; do
    while IFS= read -r -d '' f; do
        # Strip // line/doc comments first so a doc-comment mention of
        # "std::rand()" is not flagged; then match in code only.
        hits="$(sed 's://.*::' "$f" | grep -nE "$PATTERN" 2>/dev/null \
                  | grep -vE 'random_device|RAND_MAX' || true)"
        if [ -n "$hits" ]; then
            echo "❌ VIOLATION: C rand()/srand() in $f — use the RNG port instead"
            echo "$hits" | sed 's/^/   /'
            VIOLATIONS=$((VIOLATIONS + 1))
        fi
    done < <(find "$dir" \( -name '*.hpp' -o -name '*.cpp' \) -print0 2>/dev/null)
done

if [ "$VIOLATIONS" -ne 0 ]; then
    echo "no-c-rand: $VIOLATIONS violation(s) — inject randomness via IRandomSource / core::crypto::secure_random." >&2
    exit 1
fi

echo "no-c-rand: the v3 tree uses the RNG port, not C rand()/srand() — OK"
