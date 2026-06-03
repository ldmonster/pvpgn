#!/usr/bin/env bash
# check_no_singletons.sh — guard against singletons / global mutable service
# state in the domain and application layers.
#
# Per plans/04-domain-layer.md §4 and plans/08-cross-cutting.md, dependencies are
# supplied by CONSTRUCTOR INJECTION from the composition root; the domain and
# application layers must not reach for global state or singleton accessors.
# (check_domain_purity.sh already covers `domain/`; this extends the singleton
# ban to `application/`, which otherwise has no such gate, and adds the
# singleton-accessor pattern that purity does not check.)
#
# Forbidden in src/{domain,application}:
#   1. Singleton accessors:  static <T>& instance(...) / getInstance / get_instance
#   2. Calls to a singleton:  ::instance()  / getInstance(
#   3. Namespace-scope mutable globals named g_* / s_* (a deliberate naming
#      convention for "this is global mutable state").
#
# Exit 0 = GREEN, Exit 1 = RED. Allow-list is intentionally absent.

set -euo pipefail

DIRS=("src/domain" "src/application")
VIOLATIONS=0

flag() {
    local desc="$1" results="$2"
    if [ -n "$results" ]; then
        echo "❌ VIOLATION: $desc"
        echo "$results" | sed 's/^/   /'
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
}

for dir in "${DIRS[@]}"; do
    [ -d "$dir" ] || continue

    # 1. Singleton accessor declarations.
    flag "singleton accessor in $dir" \
        "$(grep -rnE 'static[^;{]*[&*][[:space:]]*(instance|getInstance|get_instance)[[:space:]]*\(' \
            "$dir" --include='*.hpp' --include='*.cpp' 2>/dev/null || true)"

    # 2. Singleton call sites.
    flag "singleton call in $dir" \
        "$(grep -rnE '(getInstance|get_instance)[[:space:]]*\(|[A-Za-z_]+::instance[[:space:]]*\(' \
            "$dir" --include='*.hpp' --include='*.cpp' 2>/dev/null || true)"

    # 3. g_/s_ named namespace-scope mutable globals (skip const/constexpr).
    flag "global mutable state (g_/s_) in $dir" \
        "$(grep -rnE '^[[:space:]]*(static[[:space:]]+)?[A-Za-z_][A-Za-z0-9_:<>]*[[:space:]]+[gs]_[A-Za-z0-9_]+[[:space:]]*(=|;|\{)' \
            "$dir" --include='*.cpp' 2>/dev/null | grep -vE 'const|constexpr' || true)"
done

if [ "$VIOLATIONS" -ne 0 ]; then
    echo "no-singletons: $VIOLATIONS violation kind(s) — inject dependencies via the constructor instead." >&2
    exit 1
fi

echo "no-singletons: domain + application are free of singletons / global mutable state — OK"
