#!/usr/bin/env python3
"""
split_handle_wol.py — Plan 15 §5: split handle_wol_link.cpp into handle_wol/ sub-modules.

Layout after split:
  src/integration/legacy_bnetd/src/
    handle_wol_link.cpp               # dispatcher + auth + welcome (~350 LOC)
    handle_wol/
      wol_internal.h                  # shared includes + forward decls (~80 LOC)
      wol_user_commands.cpp           # user/pass/privmsg/list/quit/names/part (~380 LOC)
      wol_game_commands.cpp           # joingame/gameopt/startg/host/invmsg/invdel (~450 LOC)
      wol_misc_commands.cpp           # cvers/verchk/apgar/serial/squad/buddy/ladder (~700 LOC)
"""

import os
import re

SRC = "src/integration/legacy_bnetd/src/handle_wol_link.cpp"
OUT_DIR = "src/integration/legacy_bnetd/src/handle_wol"

# ---------------------------------------------------------------------------
# Line ranges (1-based, inclusive) from the file context / grep output
# ---------------------------------------------------------------------------

# Dispatcher section: lines 1-342 (file header + extern "C" + namespace open +
# wol_strdup + typedefs + forward decls + command tables + dispatcher functions)
# plus namespace close at 1891-1893 — we'll reconstruct the dispatcher file.

# wol_user_commands.cpp: lines 344-698
USER_LINES = (344, 698)

# wol_misc_commands.cpp: lines 703-936 + 1214-1292 + 1376-1554 + 1621-1889
# (cvers/verchk/apgar/serial/squad/buddy/ladder/userip/ladder helpers)
MISC_RANGES = [(703, 936), (1214, 1292), (1376, 1554), (1621, 1889)]

# wol_game_commands.cpp: lines 938-1212 + 1294-1374 + 1556-1619
# (joingame/gameopt/startg/host/invmsg/invdel)
GAME_RANGES = [(938, 1212), (1294, 1374), (1556, 1619)]

# ---------------------------------------------------------------------------
# Read source
# ---------------------------------------------------------------------------
with open(SRC, "r", encoding="utf-8") as f:
    all_lines = f.readlines()  # 0-indexed; line N is all_lines[N-1]

def get_lines(start, end):
    """Return lines[start-1 : end] (1-based inclusive)."""
    return all_lines[start - 1 : end]

def get_ranges(ranges):
    """Concatenate multiple line ranges."""
    result = []
    for start, end in ranges:
        result.extend(get_lines(start, end))
    return result

# ---------------------------------------------------------------------------
# File header (copyright block, lines 1-19)
# ---------------------------------------------------------------------------
FILE_HEADER = "".join(get_lines(1, 19))

# ---------------------------------------------------------------------------
# Includes block (lines 21-58) — used in wol_internal.h
# ---------------------------------------------------------------------------
INCLUDES_BLOCK = "".join(get_lines(21, 58))

# ---------------------------------------------------------------------------
# Namespace open / close
# ---------------------------------------------------------------------------
NS_OPEN = "\nnamespace pvpgn\n{\n\n\tnamespace bnetd\n\t{\n\n"
NS_CLOSE = "\n\t}\n\n}\n"

# ---------------------------------------------------------------------------
# Build wol_internal.h
# ---------------------------------------------------------------------------
# All handler functions that move to sub-files need forward declarations.
# They were originally 'static' — we promote them to non-static (internal
# linkage via the shared header).

HANDLER_FORWARD_DECLS = """\
\t\t// wol_user_commands.cpp (originally static — promoted via wol_internal.h)
\t\tint _handle_user_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_pass_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_privmsg_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_list_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_quit_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_names_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_part_command(t_connection * conn, int numparams, char ** params, char * text);

\t\t// wol_misc_commands.cpp (originally static — promoted via wol_internal.h)
\t\tint _handle_cvers_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_verchk_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_apgar_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_serial_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_squadinfo_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_clanbyname_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_setopt_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_setcodepage_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_getcodepage_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_setlocale_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_getlocale_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_getinsider_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_finduser_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_finduserex_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_page_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_advertr_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_advertc_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_chanchk_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_getbuddy_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_addbuddy_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_delbuddy_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_userip_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_listsearch_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_rungsearch_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_highscore_command(t_connection * conn, int numparams, char ** params, char * text);

\t\t// wol_game_commands.cpp (originally static — promoted via wol_internal.h)
\t\tint _handle_joingame_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_gameopt_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_startg_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_host_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_invmsg_command(t_connection * conn, int numparams, char ** params, char * text);
\t\tint _handle_invdel_command(t_connection * conn, int numparams, char ** params, char * text);
"""

# wol_strdup as inline in the header so all sub-TUs can use it
WOL_STRDUP_INLINE = """\
\t\t/* xstrdup-equivalent using new char[] for paired delete[] cleanup. */
\t\tinline char* wol_strdup(char const* s)
\t\t{
\t\t\tif (!s) return nullptr;
\t\t\tstd::size_t n = std::strlen(s) + 1;
\t\t\tchar* r = new char[n];
\t\t\tstd::memcpy(r, s, n);
\t\t\treturn r;
\t\t}
"""

wol_internal_h = (
    "// SPDX-License-Identifier: GPL-2.0-or-later\n"
    "// wol_internal.h — shared forward declarations for handle_wol/ sub-modules\n"
    "// Auto-generated by scripts/dev/split_handle_wol.py (plan 15 §5)\n"
    "#pragma once\n"
    "\n"
    + INCLUDES_BLOCK
    + "\n"
    "// IRC shared handlers (from irc/irc_internal.h via irc_commands.cpp)\n"
    '#include "irc/irc_internal.h"\n'
    "\n"
    + NS_OPEN
    + WOL_STRDUP_INLINE
    + "\n"
    + HANDLER_FORWARD_DECLS
    + "\n\t} // namespace bnetd\n\n} // namespace pvpgn\n"
)

# ---------------------------------------------------------------------------
# Helper: strip 'static' from function definitions in a block
# ---------------------------------------------------------------------------
def remove_static_from_handlers(block):
    """Remove 'static' keyword from handler function definitions."""
    # Match lines like: "\t\tstatic int _handle_*" or "\t\tstatic int _ladder_*"
    # Also handle "static int append_game_info" and "struct gamelist_data"
    block = re.sub(r'(\t+)static (int _handle_)', r'\1\2', block)
    block = re.sub(r'(\t+)static (int _ladder_)', r'\1\2', block)
    block = re.sub(r'(\t+)static (int append_game_info)', r'\1\2', block)
    return block

# ---------------------------------------------------------------------------
# Build wol_user_commands.cpp
# ---------------------------------------------------------------------------
user_block = "".join(get_lines(*USER_LINES))
user_block = remove_static_from_handlers(user_block)

wol_user_cpp = (
    "// SPDX-License-Identifier: GPL-2.0-or-later\n"
    "// wol_user_commands.cpp — WoL user/pass/privmsg/list/quit/names/part handlers\n"
    "// Auto-generated by scripts/dev/split_handle_wol.py (plan 15 §5)\n"
    '#include "wol_internal.h"\n'
    + NS_OPEN
    + user_block
    + NS_CLOSE
)

# ---------------------------------------------------------------------------
# Build wol_game_commands.cpp
# ---------------------------------------------------------------------------
game_block = "".join(get_ranges(GAME_RANGES))
game_block = remove_static_from_handlers(game_block)

wol_game_cpp = (
    "// SPDX-License-Identifier: GPL-2.0-or-later\n"
    "// wol_game_commands.cpp — WoL joingame/gameopt/startg/host/invmsg/invdel handlers\n"
    "// Auto-generated by scripts/dev/split_handle_wol.py (plan 15 §5)\n"
    '#include "wol_internal.h"\n'
    + NS_OPEN
    + game_block
    + NS_CLOSE
)

# ---------------------------------------------------------------------------
# Build wol_misc_commands.cpp
# ---------------------------------------------------------------------------
misc_block = "".join(get_ranges(MISC_RANGES))
misc_block = remove_static_from_handlers(misc_block)

wol_misc_cpp = (
    "// SPDX-License-Identifier: GPL-2.0-or-later\n"
    "// wol_misc_commands.cpp — WoL cvers/verchk/apgar/serial/squad/buddy/ladder handlers\n"
    "// Auto-generated by scripts/dev/split_handle_wol.py (plan 15 §5)\n"
    '#include "wol_internal.h"\n'
    + NS_OPEN
    + misc_block
    + NS_CLOSE
)

# ---------------------------------------------------------------------------
# Rewrite handle_wol_link.cpp as dispatcher only (lines 1-342 + NS_CLOSE)
# ---------------------------------------------------------------------------
# Lines 1-19: copyright header
# Lines 21-58: includes  -> replaced by #include "handle_wol/wol_internal.h"
# Lines 60-77: comments + extern "C" declarations
# Lines 79-83: namespace open
# Lines 86-100: wol_strdup + typedefs  -> wol_strdup moves to wol_internal.h as inline
#                                         typedefs stay (needed by command tables)
# Lines 102-144: forward declarations  -> replaced by wol_internal.h
# Lines 146-342: command tables + dispatcher functions  -> stay
# Lines 1891-1893: namespace close

dispatcher_lines = (
    get_lines(1, 19)          # copyright
    + ["\n"]
    + ['#include "handle_wol/wol_internal.h"\n']
    + ["\n"]
    + get_lines(60, 77)       # comments + extern "C"
    + ["\n"]
    + get_lines(79, 83)       # namespace pvpgn { namespace bnetd {
    + ["\n"]
    # Skip wol_strdup (86-94) — now inline in wol_internal.h
    # Keep typedefs (95-100)
    + get_lines(95, 100)
    + ["\n"]
    # Skip forward declarations (102-144) — now in wol_internal.h
    # Keep command tables + dispatcher functions (146-342)
    + get_lines(146, 342)
    + ["\n"]
    + get_lines(1891, 1893)   # namespace close
)

handle_wol_dispatcher = "".join(dispatcher_lines)

# ---------------------------------------------------------------------------
# Write output files
# ---------------------------------------------------------------------------
os.makedirs(OUT_DIR, exist_ok=True)

files = {
    os.path.join(OUT_DIR, "wol_internal.h"): wol_internal_h,
    os.path.join(OUT_DIR, "wol_user_commands.cpp"): wol_user_cpp,
    os.path.join(OUT_DIR, "wol_game_commands.cpp"): wol_game_cpp,
    os.path.join(OUT_DIR, "wol_misc_commands.cpp"): wol_misc_cpp,
    SRC: handle_wol_dispatcher,
}

for path, content in files.items():
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    lines = content.count("\n")
    print(f"  wrote {path} ({lines} lines)")

print("Done.")
