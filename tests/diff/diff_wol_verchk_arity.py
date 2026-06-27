#!/usr/bin/env python3
"""Differential test: WOL CVERS/VERCHK require EXACTLY 2 middle params.

The original pvpgn-server gates both handlers on a strict arity:

    handle_wol.cpp:688  CVERS   `if (numparams == 2)`
    handle_wol.cpp:714  VERCHK  `if (numparams == 2)`

`numparams` counts only the IRC *middle* params (the trailing ':' text is
excluded). Any count != 2 yields ERR_NEEDMOREPARAMS:

    :<server> 461 <nick> <CMD> :Not enough parameters

OBSERVED DIVERGENCE (before this wave): v3 only checked for the presence of a
single space (>=2 tokens) and otherwise proceeded. So a 3-param line slipped
through:

    VERCHK 1000 1.0 extra  -> oracle 461 ; v3 379 (NONREQ)   WRONG
    CVERS  1    1000 extra -> oracle 461 ; v3 (silent)        WRONG

This pins the corrected arity gate. Both connections first send `CVERS 1 1000`
so the connection is classed WOL (mirroring the real login order); the second
line is the arity probe. We compare the SET of numeric reply codes (structure),
normalising away the server-name prefix which legitimately differs.

Run: python3 tests/diff/diff_wol_verchk_arity.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _codes(host, wolport, lines):
    """Open a fresh WOL connection, send `lines`, return the sorted list of
    numeric reply codes seen within a short window."""
    c = wc.WolClient(host, wolport)
    seen = []
    try:
        c.sock.settimeout(0.8)
        for ln in lines:
            c.send_line(ln)
        for _ in range(12):
            line = c.read_line()
            if line is None:
                break
            code = wc.WolClient.numeric(line)
            if code is not None:
                seen.append(code)
        return sorted(set(seen))
    finally:
        c.close()


# (label, lines, expected sorted set of numeric codes)
CASES = [
    # 3-param: too many middle params -> 461 on both verbs.
    ("verchk_3param", ["CVERS 1 1000", "VERCHK 1000 1.0 extra"], [461]),
    ("cvers_3param", ["CVERS 1 1000 extra"], [461]),
    # 2-param normal path still works: CVERS is silent, VERCHK -> 379.
    ("verchk_2param", ["CVERS 1 1000", "VERCHK 1000 1.0"], [379]),
    ("cvers_2param_ok", ["CVERS 1 1000"], []),
    # 1-param / no-space -> 461 (already matched; pinned for regression).
    ("verchk_1param", ["CVERS 1 1000", "VERCHK 1000"], [461]),
    # trailing-text-only middle: `VERCHK 1000 :1.0` is numparams==1 -> 461.
    ("verchk_trailing", ["CVERS 1 1000", "VERCHK 1000 :1.0"], [461]),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=13402)
    ap.add_argument("--v3-port", type=int, default=13502)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        print(f"{'case':<18}{'oracle':<14}{'v3':<14}{'expect':<14}result")
        print("-" * 70)
        all_ok = True
        for label, lines, expect in CASES:
            o = _codes("127.0.0.1", orig.wolv1_port, lines)
            n = _codes("127.0.0.1", v3.wol_port, lines)
            ok = (o == n == expect)
            all_ok &= ok
            print(f"{label:<18}{str(o):<14}{str(n):<14}{str(expect):<14}"
                  f"{'OK' if ok else 'DIFF'}")
        print()
        if all_ok:
            print("WOL CVERS/VERCHK arity gate matches the oracle.")
            return 0
        print("FAIL: WOL CVERS/VERCHK arity diverges from the oracle.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
