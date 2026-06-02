#!/usr/bin/env bash
# Plan 12 — plugin ABI purity gate (acceptance criterion: "No C++ symbol from
# domain/ or application/ is exposed to plugins").
#
# The single header a native plugin compiles against, include/pvpgn/plugin/abi.h,
# is the entire host↔plugin contract. This gate proves that boundary is pure C:
#
#   1. The header MUST compile as strict C99 (-std=c99 -pedantic-errors). If it
#      does, it cannot expose any C++ construct — a namespace / class / template
#      / reference / std:: name / overloaded symbol would not compile as C. This
#      is the definitive "no C++ symbol leaks to plugins" assertion.
#   2. Every #include in the header MUST be a C standard library header (a small
#      whitelist). This forbids dragging in any project header (domain/,
#      application/, infra/, core/, …) — which is how a C++ type would sneak
#      across the boundary in the first place.
#
# Usage: scripts/dev/check-plugin-abi-purity.sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
HEADER="$ROOT/include/pvpgn/plugin/abi.h"

[ -f "$HEADER" ] || { echo "abi-purity: missing $HEADER" >&2; exit 2; }

fail=0

# --- 1. Strict C99 compile -------------------------------------------------
CC="${CC:-cc}"
if "$CC" -std=c99 -pedantic-errors -Wall -Werror -x c -fsyntax-only "$HEADER" \
        2>/tmp/abi_purity_cc.$$; then
    echo "abi-purity: $HEADER compiles as strict C99 — OK"
else
    echo "abi-purity: FAIL — $HEADER does not compile as strict C99:" >&2
    sed 's/^/  /' /tmp/abi_purity_cc.$$ >&2
    fail=1
fi
rm -f /tmp/abi_purity_cc.$$

# --- 2. Includes must be C standard headers only ---------------------------
# Whitelist of C standard library headers the ABI may use.
allowed='stdint.h stddef.h stdbool.h stdarg.h stdint stddef'
bad=0
while IFS= read -r line; do
    # extract the included header name from #include <...> or "..."
    inc="$(printf '%s\n' "$line" | sed -nE 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]([^>"]+)[>"].*/\1/p')"
    [ -n "$inc" ] || continue
    case " $allowed " in
        *" $inc "*) : ;;  # allowed
        *)
            echo "abi-purity: FAIL — forbidden include in the public ABI: $inc" >&2
            echo "  the plugin boundary may include only C standard headers" >&2
            bad=1
            ;;
    esac
done < "$HEADER"
[ "$bad" -eq 0 ] && echo "abi-purity: all #includes are C standard headers — OK"
[ "$bad" -eq 0 ] || fail=1

exit "$fail"
