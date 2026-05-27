"""R209: strip PVPGN_V3_BNETD_INTEGRATION guards from handle_bnet_link.cpp.

Rules (we are unconditionally inside the v3 build now):
- #ifdef PVPGN_V3_BNETD_INTEGRATION ... #endif        -> keep inner, drop directives
- #ifdef PVPGN_V3_BNETD_INTEGRATION ... #else ... #endif
                                                      -> keep if-branch, drop else-branch + directives
- #ifndef PVPGN_V3_BNETD_INTEGRATION ... #endif       -> drop entire block
- #ifndef PVPGN_V3_BNETD_INTEGRATION ... #else ... #endif
                                                      -> drop if-branch, keep else-branch

Tracks nesting via a stack so unrelated #if/#ifdef/#ifndef/#endif inside the
guarded block are preserved verbatim.
"""

import re
import sys

GUARD = "PVPGN_V3_BNETD_INTEGRATION"

def process(text: str) -> str:
    lines = text.splitlines(keepends=True)
    out = []
    # Stack of dicts: { 'is_guard': bool, 'is_ifndef': bool, 'in_else': bool, 'emit': bool }
    stack = []

    re_ifdef_g  = re.compile(r"^\s*#\s*ifdef\s+" + GUARD + r"\s*$")
    re_ifndef_g = re.compile(r"^\s*#\s*ifndef\s+" + GUARD + r"\s*$")
    re_if_any   = re.compile(r"^\s*#\s*if(def|ndef)?\b")
    re_else     = re.compile(r"^\s*#\s*else\b")
    re_endif    = re.compile(r"^\s*#\s*endif\b")

    def currently_emitting() -> bool:
        # We emit a line iff every enclosing scope says emit.
        for f in stack:
            if not f['emit']:
                return False
        return True

    for line in lines:
        if re_ifdef_g.match(line):
            # Guard ifdef: emit branch, drop directive.
            parent_emit = currently_emitting()
            stack.append({'is_guard': True, 'is_ifndef': False,
                          'in_else': False, 'emit': parent_emit})
            continue  # drop the directive

        if re_ifndef_g.match(line):
            # Guard ifndef: drop branch, drop directive.
            parent_emit = currently_emitting()
            stack.append({'is_guard': True, 'is_ifndef': True,
                          'in_else': False,
                          # if parent is already not-emitting, stay not-emitting
                          'emit': False if parent_emit else False})
            # we'll flip on #else for ifndef
            continue

        if re_else.match(line):
            if stack and stack[-1]['is_guard']:
                frame = stack[-1]
                frame['in_else'] = True
                parent_emit = all(f['emit'] for f in stack[:-1])
                if frame['is_ifndef']:
                    # ifndef-guard's else: emit (under parent)
                    frame['emit'] = parent_emit
                else:
                    # ifdef-guard's else: drop
                    frame['emit'] = False
                continue  # drop the #else
            else:
                # Non-guard #else: pass through, just toggle nothing
                if currently_emitting():
                    out.append(line)
                continue

        if re_endif.match(line):
            if stack and stack[-1]['is_guard']:
                stack.pop()
                continue  # drop the #endif
            else:
                if currently_emitting():
                    out.append(line)
                continue

        if re_if_any.match(line):
            # Non-guard #if/#ifdef/#ifndef -- track as transparent frame.
            parent_emit = currently_emitting()
            stack.append({'is_guard': False, 'is_ifndef': False,
                          'in_else': False, 'emit': parent_emit})
            if parent_emit:
                out.append(line)
            continue

        if currently_emitting():
            out.append(line)

    return "".join(out)


if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "r", encoding="utf-8", newline="") as f:
        text = f.read()
    result = process(text)
    with open(dst, "w", encoding="utf-8", newline="") as f:
        f.write(result)
    print(f"wrote {dst} ({len(result.splitlines())} lines from {len(text.splitlines())})")
