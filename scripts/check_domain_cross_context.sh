#!/usr/bin/env bash
# check_domain_cross_context.sh — guard against cross-context coupling.
#
# Each directory under src/domain/<ctx> is a bounded context. Per
# plans/04-domain-layer.md §3 and its Definition of Done, a context must NOT
# #include another context's internal headers — the only allowed cross-context
# link is the shared kernel (domain/shared) plus domain events handled in the
# application layer. This check fails if any domain/<a> file includes
# domain/<b>/... for a different context b.
#
# Exit 0 = GREEN (no cross-context includes), Exit 1 = RED (violations found).
#
# The allow-list below is intentionally EMPTY and may only shrink. If a genuine
# shared concept is found, promote it to domain/shared rather than allow-listing.

set -euo pipefail

DOMAIN_DIR="${1:-src/domain}"
VIOLATIONS=0

# Allow-list: "<from-ctx>|<included-ctx>" pairs that are temporarily tolerated.
# MUST stay empty in the committed tree (allow-lists only shrink).
ALLOW=""

for ctx_path in "$DOMAIN_DIR"/*/; do
    ctx="$(basename "$ctx_path")"
    [ "$ctx" = "shared" ] && continue

    # All includes of some domain/<other>/... within this context's files.
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        file="${line%%:*}"
        inc="$(printf '%s\n' "$line" | grep -oE 'domain/[a-z0-9_]+/' | head -1)"
        other="$(printf '%s\n' "$inc" | sed -E 's#domain/([a-z0-9_]+)/#\1#')"

        # Same context or the shared kernel are fine.
        [ "$other" = "$ctx" ] && continue
        [ "$other" = "shared" ] && continue

        # Allow-list check.
        case " $ALLOW " in
            *" ${ctx}|${other} "*) continue ;;
        esac

        echo "❌ cross-context include: domain/${ctx} -> domain/${other}"
        echo "   $line" | sed 's/^/   /'
        VIOLATIONS=$((VIOLATIONS + 1))
    done < <(grep -rnE '#include "domain/[a-z0-9_]+/' "$ctx_path" \
                 --include="*.hpp" --include="*.cpp" 2>/dev/null || true)
done

if [ "$VIOLATIONS" -ne 0 ]; then
    echo "cross-context: $VIOLATIONS violation(s) — promote shared concepts to domain/shared." >&2
    exit 1
fi

echo "cross-context: no domain context includes another's internals — OK"
