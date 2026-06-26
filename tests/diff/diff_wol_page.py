#!/usr/bin/env python3
"""Differential test: WOL PAGE user-to-user paging.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client B: WOL login
    client A: WOL login
    client A: PAGE pageb :ping       -> 389 "0 :"  (target online, pageme on)
    client A: PAGE ghost :ping       -> 389 "1 :"  (target offline/unknown)

and diffs the 389 status ('0' paged / '1' not) for an online and an unknown
target. A match means v3 reports WOL paging success the way the original does.
(The page-delivery line to the target is sent via the cross-session router but
its exact bytes are not compared — the 389 status is the behavioural contract.)

Run: python3 tests/diff/diff_wol_page.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def scenario(host, port, sku=1000):
    b = wc.wol_session(host, port, "pageb", "secretpass", sku=sku)
    a = wc.wol_session(host, port, "pagea", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"online": None, "unknown": None}
    try:
        online = wc.wol_page(a, "pageb", "ping")
        unknown = wc.wol_page(a, "ghostpage_nope", "ping")
        return {"online": online, "unknown": unknown}
    finally:
        a.close()
        b.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6420)
    ap.add_argument("--v3-port", type=int, default=6520)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<10}{'original':<12}{'v3':<12}match")
        print("-" * 44)
        ok = True
        for k in ("online", "unknown"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<10}{ov:<12}{nv:<12}{'OK' if m else 'DIFF'}")
        print()

        if ok and o["online"] == "0" and o["unknown"] == "1":
            print("WOL PAGE matches the oracle (online paged, unknown not).")
            return 0
        print("FAIL: WOL PAGE divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
