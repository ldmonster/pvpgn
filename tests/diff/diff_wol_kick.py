#!/usr/bin/env python3
"""Differential test: WOL KICK #chan <nick> (operator removes a member).

KICK was a silent no-op in v3 (wol_known[]). The original lets a channel operator
kick a member: a KICK line is broadcast to the channel (including the victim) and
the victim is removed. v3 now implements WolFsm::on_kick (operator-gated via
channel.operator_id(), broadcast via route_irc_line, removal via LeaveChannel).

The exact KICK prefix is environment-dependent (the oracle uses WCHT@<ip>; v3 uses
@Battle.net), so this compares the decisive observables: the victim receives a
KICK naming them, the operator sees it, and the victim is gone from the channel
roster afterward. Joins are ORDERED so the operator is deterministic.

Run: python3 tests/diff/diff_wol_kick.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.7):
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


def scenario(host, port):
    op = wc.wol_session(host, port, "kop", "pw")
    if op is None:
        return None
    op.send_line("JOIN #K")
    time.sleep(0.4)
    _drain(op)  # kop is the operator (first joiner)
    vic = wc.wol_session(host, port, "kvic", "pw")
    if vic is None:
        op.close()
        return None
    vic.send_line("JOIN #K")
    time.sleep(0.4)
    _drain(op)
    _drain(vic)

    op.send_line("KICK #K kvic :Bye")
    vic_lines = _drain(vic)
    op_lines = _drain(op)
    op.send_line("NAMES #K")
    names = [l for l in _drain(op) if " 353 " in l]

    op.close()
    vic.close()
    victim_in_roster = any("kvic" in l for l in names)
    return {
        "victim_got_kick": any(" KICK " in l and "kvic" in l for l in vic_lines),
        "op_saw_kick": any(" KICK " in l for l in op_lines),
        "victim_removed": not victim_in_roster,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6404)
    ap.add_argument("--v3-port", type=int, default=6504)
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
        fields = ["victim_got_kick", "op_saw_kick", "victim_removed"]
        print(f"{'field':<18}{'oracle':<10}{'v3':<10}match")
        print("-" * 46)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<18}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        success = all_ok and all(o[f] for f in fields)
        if success:
            print("WOL KICK matches the oracle (victim kicked + removed).")
            return 0
        print("FAIL: WOL KICK divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
