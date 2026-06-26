#!/usr/bin/env python3
"""Differential test: WOL NAMES <channel> (RPL_NAMREPLY 353 + RPL_ENDOFNAMES 366).

The original answers NAMES <channel> with a 353 listing the channel's members —
the channel operator prefixed with '@' — followed by a 366. v3 previously listed
NAMES in its silent-accept set (wol_known[]) and replied nothing.

Joins are ORDERED (A joins and settles, then B joins) so the operator is
deterministic on both servers — the FIRST joiner of a fresh non-permanent channel
is its operator (matching the channel-operator model). The 353 member ORDER is
unordered-map-dependent, so this compares the member SET and the operator marking,
not the literal string.

Run: python3 tests/diff/diff_wol_names.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.8):
    time.sleep(settle)
    c.sock.settimeout(0.5)
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


def _parse_names(lines, channel):
    """Return (members_set, got_353, got_366). Members include the '@' prefix."""
    members = set()
    got_353 = got_366 = False
    for line in lines:
        parts = line.split()
        if len(parts) < 3:
            continue
        code = parts[1]
        if code == "353" and channel.lower() in line.lower():
            got_353 = True
            colon = line.find(" :")
            if colon >= 0:
                for m in line[colon + 2:].split():
                    members.add(m)
        elif code == "366" and channel.lower() in line.lower():
            got_366 = True
    return members, got_353, got_366


def scenario(host, port):
    chan = "#NamesCh"
    a = wc.wol_session(host, port, "alfa", "pw")
    if a is None:
        return None
    a.send_line(f"JOIN {chan}")
    time.sleep(0.4)
    _drain(a)  # alfa is now the operator (first joiner)
    b = wc.wol_session(host, port, "bravo", "pw")
    if b is None:
        a.close()
        return None
    b.send_line(f"JOIN {chan}")
    time.sleep(0.4)
    _drain(a)
    a.send_line(f"NAMES {chan}")
    members, got_353, got_366 = _parse_names(_drain(a), chan)
    a.close()
    b.close()
    return {
        "got_353": got_353,
        "got_366": got_366,
        "op_is_alfa": "@alfa" in members,
        "bravo_plain": "bravo" in members,
        "member_count": len(members),
    }


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
        fields = ["got_353", "got_366", "op_is_alfa", "bravo_plain", "member_count"]
        print(f"{'field':<16}{'oracle':<12}{'v3':<12}match")
        print("-" * 48)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<16}{str(o[f]):<12}{str(n[f]):<12}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["got_353"] and o["got_366"]
                   and o["op_is_alfa"] and o["bravo_plain"])
        if success:
            print("WOL NAMES matches the oracle (353 roster w/ operator '@' + 366).")
            return 0
        print("FAIL: WOL NAMES divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
