#!/usr/bin/env python3
"""Differential test: WOL TIME (391) and MODE query (324/368/501).

Both were silent no-ops in v3 (wol_known[]). The original replies:
  TIME            -> 391 RPL_TIME  ":server 391 <nick> <server> :<unixtime>"
  MODE #chan      -> 324 RPL_CHANNELMODEIS  ":server 324 <nick> #chan +tns"
  MODE #chan b    -> 368 RPL_ENDOFBANLIST (empty)
  MODE <nick>     -> 501 ERR_UMODEUNKNOWNFLAG

Server name + time value are environment-dependent, so this compares the numeric
codes (and the "+tns" mode string), not the literal server/time.

Run: python3 tests/diff/diff_wol_timemode.py
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


def _codes(lines):
    out = []
    for l in lines:
        p = l.split()
        if len(p) > 1 and p[1].isdigit():
            out.append(p[1])
    return out


def scenario(host, port):
    c = wc.wol_session(host, port, "tmu", "pw")
    if c is None:
        return None
    c.send_line("TIME")
    time_codes = _codes(_drain(c))
    c.send_line("JOIN #M")
    time.sleep(0.3)
    _drain(c)
    c.send_line("MODE #M")
    mq = _drain(c)
    c.send_line("MODE #M b")
    mb = _codes(_drain(c))
    c.send_line("MODE tmu")
    mu = _codes(_drain(c))
    c.close()
    return {
        "time_391": "391" in time_codes,
        "mode_324_tns": any(" 324 " in l and "+tns" in l for l in mq),
        "mode_b_368": "368" in mb,
        "mode_user_501": "501" in mu,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6402)
    ap.add_argument("--v3-port", type=int, default=6502)
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
        fields = ["time_391", "mode_324_tns", "mode_b_368", "mode_user_501"]
        print(f"{'field':<16}{'oracle':<10}{'v3':<10}match")
        print("-" * 44)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<16}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        success = all_ok and all(o[f] for f in fields)
        if success:
            print("WOL TIME + MODE query match the oracle.")
            return 0
        print("FAIL: WOL TIME/MODE divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
