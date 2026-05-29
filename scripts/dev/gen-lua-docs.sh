#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# scripts/dev/gen-lua-docs.sh
#
# Generates docs/lua-api-reference.md from:
#   src/v3/infra/scripting/src/lua_api_v2.cpp
#
# Extracts -- doc comments above each pvpgn.* registration and formats them
# as a Markdown reference page.
#
# Output: docs/lua-api-reference.md
#
# Usage:
#   bash scripts/dev/gen-lua-docs.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SOURCE_FILE="${REPO_ROOT}/src/v3/infra/scripting/src/lua_api_v2.cpp"
OUTPUT_FILE="${REPO_ROOT}/docs/lua-api-reference.md"

# ---- Helpers ----------------------------------------------------------------

log() { echo "[gen-lua-docs] $*"; }

check_file() {
    if [[ ! -f "$1" ]]; then
        echo "[gen-lua-docs] ERROR: source file not found: $1" >&2
        exit 1
    fi
}

# ---- Main -------------------------------------------------------------------

check_file "${SOURCE_FILE}"
mkdir -p "$(dirname "${OUTPUT_FILE}")"

log "Generating docs/lua-api-reference.md from lua_api_v2.cpp..."

# Write the file header
cat > "${OUTPUT_FILE}" << 'HEADER'
# PvPGN Lua API v2 — Auto-generated Reference

> **Auto-generated** from `src/v3/infra/scripting/src/lua_api_v2.cpp`
> by `scripts/dev/gen-lua-docs.sh`.  Do not edit manually.
>
> For the full narrative documentation including examples and migration guide,
> see [`docs/lua-api-v2.md`](lua-api-v2.md).

---

## Functions

HEADER

# Extract doc comment blocks (lines starting with "    // --") and the
# pvpgn.set_function("name", ...) call that follows them.
#
# Pattern in lua_api_v2.cpp:
#   // ---- pvpgn.foo(args) ----
#   // -- description
#   pvpgn.set_function("foo", ...)
#
awk '
BEGIN {
    in_block = 0
    block_header = ""
    block_body = ""
    fn_name = ""
}

# Detect separator lines like: "    // ---- pvpgn.foo(args) ----"
/^[[:space:]]*\/\/ -{4,}/ {
    # Extract the function signature from the separator
    line = $0
    gsub(/^[[:space:]]*\/\/ -+[[:space:]]*/, "", line)
    gsub(/[[:space:]]*-+[[:space:]]*$/, "", line)
    if (line != "" && line ~ /^pvpgn\./) {
        block_header = line
        block_body = ""
        in_block = 1
    }
    next
}

# Accumulate doc comment lines inside a block
in_block && /^[[:space:]]*\/\/ --/ {
    line = $0
    gsub(/^[[:space:]]*\/\/ -- ?/, "", line)
    block_body = block_body line "\n"
    next
}

# Detect pvpgn.set_function("name", ...) — emit the section
in_block && /pvpgn\.set_function\(/ {
    match($0, /pvpgn\.set_function\("([^"]+)"/, arr)
    fn_name = arr[1]
    if (block_header != "") {
        print "### `" block_header "`\n"
        if (block_body != "") {
            print block_body
        }
        print "**Handler key:** `pvpgn." fn_name "`\n"
        print "---\n"
    }
    in_block = 0
    block_header = ""
    block_body = ""
    fn_name = ""
    next
}

# Reset if we hit a non-comment, non-function line while in a block
in_block && !/^[[:space:]]*\/\// && !/^[[:space:]]*$/ {
    in_block = 0
    block_header = ""
    block_body = ""
}
' "${SOURCE_FILE}" >> "${OUTPUT_FILE}"

# Append footer with links
cat >> "${OUTPUT_FILE}" << 'FOOTER'

---

## Handler Map Keys

When registering C++ handlers for Lua API v2 functions, use these keys:

| Lua function              | Handler map key          |
|---------------------------|--------------------------|
| `pvpgn.log`               | `"pvpgn.log"`            |
| `pvpgn.send_chat`         | `"pvpgn.send_chat"`      |
| `pvpgn.get_account`       | `"pvpgn.get_account"`    |
| `pvpgn.ban_account`       | `"pvpgn.ban_account"`    |
| `pvpgn.kick_user`         | `"pvpgn.kick_user"`      |
| `pvpgn.broadcast`         | `"pvpgn.broadcast"`      |

## See Also

- [`docs/lua-api-v2.md`](lua-api-v2.md) — Full narrative documentation
- [`docs/sandbox-integration-guide.md`](sandbox-integration-guide.md) — Sandboxing
- [`src/v3/infra/scripting/src/lua_api_v2.cpp`](../src/v3/infra/scripting/src/lua_api_v2.cpp) — Source
FOOTER

log "Written: ${OUTPUT_FILE}"
log "Done."
