#!/usr/bin/env bash
# check_domain_purity.sh — CI guard for domain layer purity
# Checks that src/v3/domain/ contains no forbidden patterns.
# Exit 0 = GREEN (all clean), Exit 1 = RED (violations found).
#
# Forbidden patterns (per plans/03-domain-purification.md §6):
#   1. #include "infra/..."  or  #include <infra/...>   — no infra deps in domain
#   2. #include <iostream>                               — no I/O streams
#   3. #include <fstream>                                — no file streams
#   4. #include <cstdio>                                 — no C stdio
#   5. #include <ctime>                                  — no C time (use core::SystemTime)
#   6. std::cout / std::cerr / std::cin                  — no direct I/O
#   7. printf( / fprintf( / sprintf( / snprintf(         — no C-style I/O
#   8. system_clock::now()                               — no direct clock calls (inject time)
#   9. static .* g_                                      — no global mutable state
#  10. spdlog::                                          — no logging library in domain

set -euo pipefail

DOMAIN_DIR="${1:-src/v3/domain}"
VIOLATIONS=0

check() {
    local description="$1"
    local pattern="$2"
    local results
    results=$(grep -rn --include="*.hpp" --include="*.cpp" -E "$pattern" "$DOMAIN_DIR" 2>/dev/null || true)
    if [[ -n "$results" ]]; then
        echo "❌ VIOLATION: $description"
        echo "$results" | sed 's/^/   /'
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
}

echo "🔍 Checking domain purity in: $DOMAIN_DIR"
echo ""

check "infra/ include in domain"          '#include[[:space:]]*("|<)(infra/)'
check "<iostream> in domain"              '#include[[:space:]]*<iostream>'
check "<fstream> in domain"              '#include[[:space:]]*<fstream>'
check "<cstdio> in domain"               '#include[[:space:]]*<cstdio>'
check "<ctime> in domain"                '#include[[:space:]]*<ctime>'
check "std::cout/cerr/cin in domain"     'std::(cout|cerr|cin)[[:space:]]*[<|]'
check "C-style I/O in domain"            '\b(printf|fprintf|sprintf|snprintf)[[:space:]]*\('
check "system_clock::now() in domain"    'system_clock::now\(\)'
check "global mutable state in domain"   'static[[:space:]].*[[:space:]]g_[a-zA-Z]'
check "spdlog:: in domain"               'spdlog::'

echo ""
if [[ $VIOLATIONS -eq 0 ]]; then
    echo "✅ Domain purity check PASSED — zero violations found."
    exit 0
else
    echo "❌ Domain purity check FAILED — $VIOLATIONS violation(s) found."
    exit 1
fi
