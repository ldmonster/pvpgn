#!/usr/bin/env python3
"""Differential test: WOL LIST param-count gating (channels vs. empty envelope).

The original _handle_list_command (handle_wol.cpp) decides what to list from the
COUNT and EQUALITY of the middle params:
  - numparams == 0                    -> list chat channels (and games)
  - numparams == 2 && p[0] != p[1]    -> list chat channels only
  - numparams == 2 && p[0] == p[1]    -> list games only (no channels)
  - any other count (1, 3, ...)       -> list NEITHER (just 321 + 323 envelope)

v3 previously listed channels regardless of the params, so "LIST 0", "LIST 0 0"
and "LIST 0 0 0" wrongly returned the channel set where the oracle returns just
the 321/323 envelope. This guard joins a user channel (#PListCh) and checks, for
each LIST variant, whether that channel's 327 entry appears: it must appear for
the bare and two-unequal-param forms and must NOT appear for the one/two-equal/
three-param forms — matching the oracle. (No games exist, so the games-only
two-equal case collapses to the empty envelope on both servers.)

Run: python3 tests/diff/diff_wol_list_params.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.6):
    time.sleep(settle)
    c.sock.settimeout(0.4)
    out = []
    try:
        while True:
            line = c.read_line()
            if line is None:
                break
            out.append(line)
    except OSError:
        pass
    return out


def _observe(lines, chan):
    """Return (got_321, got_323, chan_count) for a LIST reply."""
    g321 = g323 = False
    n = 0
    for line in lines:
        p = line.split()
        if len(p) < 2:
            continue
        if p[1] == "321":
            g321 = True
        elif p[1] == "323":
            g323 = True
        elif p[1] == "327" and len(p) >= 4 and p[3] == chan:
            n += 1
    return (g321, g323, n)


VARIANTS = [
    ("bare", "LIST"),
    ("one", "LIST 0"),
    ("two_eq", "LIST 0 0"),
    ("two_ne", "LIST 0 1"),
    ("three", "LIST 0 0 0"),
]


def scenario(host, port):
    chan = "#PListCh"
    a = wc.wol_session(host, port, "alfa", "pw")
    if a is None:
        return None
    a.send_line(f"JOIN {chan}")
    time.sleep(0.4)
    _drain(a)
    res = {}
    for label, cmd in VARIANTS:
        a.send_line(cmd)
        res[label] = _observe(_drain(a), chan)
    a.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6398)
    ap.add_argument("--v3-port", type=int, default=6498)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        if not o or not n:
            print("FAIL: login/setup failed")
            return 1
        print(f"{'variant':<10}{'oracle(321,323,#ch)':<26}{'v3':<26}match")
        print("-" * 70)
        all_ok = True
        for label, _ in VARIANTS:
            same = o[label] == n[label]
            all_ok &= same
            print(f"{label:<10}{str(o[label]):<26}{str(n[label]):<26}"
                  f"{'OK' if same else 'DIFF'}")
        print()
        # Sanity: the oracle must actually exhibit the gating we are guarding.
        oracle_gates = (o["bare"][2] == 1 and o["two_ne"][2] == 1
                        and o["one"][2] == 0 and o["two_eq"][2] == 0
                        and o["three"][2] == 0)
        if all_ok and oracle_gates:
            print("WOL LIST param-count gating matches the oracle.")
            return 0
        print("FAIL: WOL LIST param-count divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
