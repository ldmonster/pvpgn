#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# scripts/dev/gen-plugin-docs.sh
#
# Generates docs/plugin-api.md from src/v3/infra/plugin/include/infra/plugin/api.h
#
# Strategy:
#   1. If doxygen is available, run it with a minimal Doxyfile targeting the
#      plugin include directory and convert the XML output to Markdown.
#   2. Otherwise, use awk to extract /** ... */ doc comments from api.h and
#      format them as Markdown.
#
# Output: docs/plugin-api.md
#
# Usage:
#   bash scripts/dev/gen-plugin-docs.sh
#   bash scripts/dev/gen-plugin-docs.sh --doxygen-only
#   bash scripts/dev/gen-plugin-docs.sh --awk-only

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

API_HEADER="${REPO_ROOT}/src/v3/infra/plugin/include/infra/plugin/api.h"
OUTPUT_FILE="${REPO_ROOT}/docs/plugin-api.md"
DOXYGEN_TMPDIR="${REPO_ROOT}/build/doxygen-plugin-tmp"

MODE="${1:-auto}"  # auto | --doxygen-only | --awk-only

# ---- Helpers ----------------------------------------------------------------

log() { echo "[gen-plugin-docs] $*"; }

check_file() {
    if [[ ! -f "$1" ]]; then
        echo "[gen-plugin-docs] ERROR: source file not found: $1" >&2
        exit 1
    fi
}

# ---- awk-based extraction ---------------------------------------------------

generate_with_awk() {
    log "Generating docs/plugin-api.md using awk extraction..."

    cat > "${OUTPUT_FILE}" << 'HEADER'
# PvPGN Plugin C ABI 1.0 Reference

> **Auto-generated** from `src/v3/infra/plugin/include/infra/plugin/api.h`
> by `scripts/dev/gen-plugin-docs.sh`.  Do not edit manually.

---

HEADER

    # Extract /** ... */ blocks and the declaration that follows them.
    awk '
    BEGIN {
        in_comment = 0
        comment = ""
        decl = ""
    }

    # Start of a doc comment
    /^\/\*\*/ {
        in_comment = 1
        comment = ""
        next
    }

    # End of a doc comment
    in_comment && /\*\// {
        in_comment = 0
        # Strip leading " * " from each line
        gsub(/^ \* ?/, "", comment)
        next
    }

    # Inside a doc comment — accumulate lines
    in_comment {
        line = $0
        sub(/^ \* ?/, "", line)
        comment = comment line "\n"
        next
    }

    # After a comment block, capture the next non-empty line as the declaration
    !in_comment && comment != "" && /\S/ {
        decl = $0
        # Emit as Markdown
        print "### `" decl "`\n"
        print comment
        print "---\n"
        comment = ""
        decl = ""
        next
    }
    ' "${API_HEADER}" >> "${OUTPUT_FILE}"

    # Append the version constant and macro documentation
    cat >> "${OUTPUT_FILE}" << 'FOOTER'

## Version Constant

```c
#define PVPGN_PLUGIN_API_VERSION 1
```

The loader checks that `pvpgn_plugin_info_t::api_version` equals this value.
Plugins with a different version are logged and skipped.

## Convenience Macro

```c
PVPGN_PLUGIN_EXPORT_INFO(&my_info)
```

Expands to the `pvpgn_plugin_get_info()` function definition.  Use it in your
plugin's `.c` file to avoid boilerplate.

## See Also

- [`docs/sandbox-integration-guide.md`](sandbox-integration-guide.md)
- [`docs/lua-api-v2.md`](lua-api-v2.md)
- [`plugins/example-quiz/native/main.c`](../plugins/example-quiz/native/main.c)
FOOTER

    log "Written: ${OUTPUT_FILE}"
}

# ---- doxygen-based extraction -----------------------------------------------

generate_with_doxygen() {
    log "Generating docs/plugin-api.md using doxygen..."

    mkdir -p "${DOXYGEN_TMPDIR}"

    # Write a minimal Doxyfile
    cat > "${DOXYGEN_TMPDIR}/Doxyfile" << DOXYFILE
PROJECT_NAME           = "PvPGN Plugin API"
PROJECT_NUMBER         = "1.0"
OUTPUT_DIRECTORY       = ${DOXYGEN_TMPDIR}
INPUT                  = ${REPO_ROOT}/src/v3/infra/plugin/include/infra/plugin/api.h
GENERATE_HTML          = NO
GENERATE_LATEX         = NO
GENERATE_XML           = YES
XML_OUTPUT             = xml
QUIET                  = YES
WARNINGS               = NO
EXTRACT_ALL            = YES
DOXYFILE

    doxygen "${DOXYGEN_TMPDIR}/Doxyfile" 2>/dev/null || true

    # If doxygen produced XML, convert it; otherwise fall back to awk.
    if [[ -d "${DOXYGEN_TMPDIR}/xml" ]]; then
        log "Doxygen XML generated; converting to Markdown..."

        cat > "${OUTPUT_FILE}" << 'HEADER'
# PvPGN Plugin C ABI 1.0 Reference

> **Auto-generated** from `src/v3/infra/plugin/include/infra/plugin/api.h`
> by `scripts/dev/gen-plugin-docs.sh` (doxygen).  Do not edit manually.

---

HEADER

        # Extract brief descriptions from doxygen XML using awk/grep
        if command -v xmllint &>/dev/null; then
            xmllint --xpath "//memberdef[@kind='typedef']/name/text()" \
                "${DOXYGEN_TMPDIR}/xml/api_8h.xml" 2>/dev/null \
                | tr ' ' '\n' \
                | while read -r name; do
                    echo "- \`${name}\`" >> "${OUTPUT_FILE}"
                done
        fi

        # Append the awk-extracted comments as well for completeness
        generate_with_awk

        rm -rf "${DOXYGEN_TMPDIR}"
    else
        log "Doxygen XML not produced; falling back to awk extraction."
        rm -rf "${DOXYGEN_TMPDIR}"
        generate_with_awk
    fi
}

# ---- Main -------------------------------------------------------------------

check_file "${API_HEADER}"
mkdir -p "$(dirname "${OUTPUT_FILE}")"

case "${MODE}" in
    --awk-only)
        generate_with_awk
        ;;
    --doxygen-only)
        if ! command -v doxygen &>/dev/null; then
            echo "[gen-plugin-docs] ERROR: doxygen not found and --doxygen-only specified." >&2
            exit 1
        fi
        generate_with_doxygen
        ;;
    auto|*)
        if command -v doxygen &>/dev/null; then
            generate_with_doxygen
        else
            log "doxygen not found; using awk fallback."
            generate_with_awk
        fi
        ;;
esac

log "Done."
