#!/usr/bin/env python3
"""Convert std::fprintf/std::printf calls in tools/* to std::print/std::println.

Conservative: only rewrites a call when its format argument is a *single* string
literal (no adjacent-literal concatenation, no dynamic `%*`/`%.*`). Calls that
don't qualify are left untouched and reported, so they can be handled by hand.

printf %-spec  -> std::format {}-spec translation:
  %%            -> literal %
  %[flags][width][.prec][len]<conv>:
     flags  - -> '<' (left align); 0 -> '0'; + -> '+'; ' ' -> ' '; # -> '#'
     width/prec preserved
     length (hh/h/l/ll/z/j/t/L) dropped
     conv   d/i/u -> (none); x->x; X->X; o->o; f/F->f; e/E->e; g/G->g;
            c/s/p -> (none)
A trailing "\\n" in the format makes the call std::println (newline implicit).
Literal '{' / '}' in the format are escaped to '{{' / '}}'.
"""
import re
import sys

SPEC = re.compile(
    r'%([-+ #0]*)(\d+)?(?:\.(\d+))?(?:hh|h|ll|l|z|j|t|L)?([diouxXeEfFgGaAcspn%])')


def conv_one(m):
    flags, width, prec, conv = m.groups()
    if conv == '%':
        return '%'
    align = ''
    fmtflags = ''
    for f in flags:
        if f == '-':
            align = '<'
        elif f == '0':
            fmtflags += '0'
        elif f in '+ #':
            fmtflags += f
    typ = ''
    if conv in 'xXoeEfFgG':
        typ = conv.lower() if conv in 'eEfFgG' else conv
        if conv in 'eEgG':
            typ = conv.lower()
    spec = ''
    if align or fmtflags or width or prec or typ:
        spec = ':' + align + fmtflags + (width or '')
        if prec:
            spec += '.' + prec
        spec += typ
    return '{' + spec + '}'


def translate_format(lit_body):
    # lit_body is the raw text between the quotes (with escapes intact).
    # Escape braces first (they are literal in printf), then convert specs.
    out = lit_body.replace('{', '{{').replace('}', '}}')
    out = SPEC.sub(conv_one, out)
    return out


STR = re.compile(r'"((?:[^"\\]|\\.)*)"')


def find_call_end(text, open_idx):
    """Given index of '(' return index just past the matching ')'."""
    depth = 0
    i = open_idx
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == '\\':
                    i += 1
                i += 1
        elif c == "'":
            i += 1
            while i < n and text[i] != "'":
                if text[i] == '\\':
                    i += 1
                i += 1
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def process(path):
    with open(path, encoding='utf-8') as f:
        text = f.read()
    skipped = []
    out = []
    i = 0
    for mname in ['std::fprintf(', 'std::printf(']:
        pass
    # single pass scan
    pat = re.compile(r'std::(f?)printf\s*\(')
    pos = 0
    result = []
    while True:
        m = pat.search(text, pos)
        if not m:
            result.append(text[pos:])
            break
        result.append(text[pos:m.start()])
        is_f = m.group(1) == 'f'
        open_idx = m.end() - 1
        end = find_call_end(text, open_idx)
        if end < 0:
            result.append(text[m.start():m.end()])
            pos = m.end()
            continue
        call = text[m.start():end]            # std::fprintf(...) up to ')'
        inner = text[open_idx + 1:end - 1]    # arguments
        # Split off the stream arg for fprintf.
        rest = inner
        stream = ''
        if is_f:
            # first arg up to first top-level comma
            depth = 0
            j = 0
            while j < len(inner):
                c = inner[j]
                if c == '"':
                    j += 1
                    while j < len(inner) and inner[j] != '"':
                        if inner[j] == '\\':
                            j += 1
                        j += 1
                elif c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                elif c == ',' and depth == 0:
                    break
                j += 1
            stream = inner[:j].strip()
            rest = inner[j + 1:]
        rest_strip = rest.lstrip()
        sm = STR.match(rest_strip)
        # Qualify: format must be a single string literal, optionally followed
        # by ',' args or ')'. Reject concatenation (literal followed by another
        # token that continues the format) and dynamic specs.
        ok = False
        if sm:
            after = rest_strip[sm.end():].lstrip()
            body = sm.group(1)
            if (after == '' or after.startswith(',')) and '%*' not in body \
                    and '%.*' not in body and '%-*' not in body \
                    and not re.search(r'%[-+ #0-9.]*\*', body):
                ok = True
        if not ok:
            skipped.append(m.start())
            result.append(call)
            pos = end
            continue
        body = sm.group(1)
        args_tail = rest_strip[sm.end():].lstrip()  # ',' + rest, or ''
        newline = False
        if body.endswith('\\n'):
            body2 = body[:-2]
            newline = True
        else:
            body2 = body
        fmt = translate_format(body2)
        func = 'std::println' if newline else 'std::print'
        pieces = [func, '(']
        if is_f:
            pieces.append(stream + ', ')
        pieces.append('"' + fmt + '"')
        if args_tail.startswith(','):
            pieces.append(args_tail)  # includes leading comma
        pieces.append(')')
        result.append(''.join(pieces))
        pos = end

    newtext = ''.join(result)
    if newtext != text:
        # ensure <print> included
        if '#include <print>' not in newtext:
            newtext = re.sub(r'(#include <cstdio>\n)',
                             r'\1#include <print>\n', newtext, count=1)
            if '#include <print>' not in newtext:
                # fall back: add after first #include
                newtext = re.sub(r'(#include [<"][^>"]+[>"]\n)',
                                 r'\1#include <print>\n', newtext, count=1)
        with open(path, 'w', encoding='utf-8') as f:
            f.write(newtext)
    return len(skipped)


if __name__ == '__main__':
    total_skip = 0
    for p in sys.argv[1:]:
        s = process(p)
        total_skip += s
        print(f'{p}: {s} call(s) left for manual handling')
    print('total skipped:', total_skip)
