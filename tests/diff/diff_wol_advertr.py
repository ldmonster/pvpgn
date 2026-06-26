#!/usr/bin/env python3
"""Differential test: WOL ADVERTR game-ad refresh ack.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client A: ADVERTR #adroom   -> ":<server> ADVERTR 5 #adroom"

and diffs the ADVERTR reply payload. A match means v3 reproduces the WOL ADVERTR
ack the way the original does.

Run: python3 tests/diff/diff_wol_advertr.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

ROOM = "adroom"


def scenario(host, port, sku=1000):
    c = wc.wol_session(host, port, "advuser", "secretpass", sku=sku)
    if c is None:
        return {"advertr": None}
    try:
        return {"advertr": wc.wol_advertr(c, f"#{ROOM}")}
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6432)
    ap.add_argument("--v3-port", type=int, default=6532)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<10}{'original':<14}{'v3':<14}match")
        print("-" * 46)
        ov, nv = str(o["advertr"]), str(n["advertr"])
        m = ov == nv
        print(f"{'advertr':<10}{ov:<14}{nv:<14}{'OK' if m else 'DIFF'}")
        print()

        if m and o["advertr"] == f"5 #{ROOM}":
            print("WOL ADVERTR matches the oracle.")
            return 0
        print("FAIL: WOL ADVERTR divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
