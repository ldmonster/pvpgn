#!/usr/bin/env python3
"""Differential test: WOL channel membership cleanup on disconnect.

    A,B: WOL login -> JOIN #wdisc
    B: LIST -> #wdisc count == 2
    A: disconnect
    B: LIST -> #wdisc count should drop to 1 (A removed)

Diffs the post-disconnect member count. A divergence means v3 leaves the
disconnected client as a ghost member (WolFsm::on_close does no channel leave).

Run: python3 tests/diff/diff_wol_disconnect.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

CHAN = "wdisc"


def scenario(host, port, sku=1000):
    a = wc.wol_session(host, port, "wdisca", "pw", sku=sku)
    b = wc.wol_session(host, port, "wdiscb", "pw", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"before": None, "after": None}
    try:
        wc.wol_join(a, f"#{CHAN}")
        wc.wol_join(b, f"#{CHAN}")
        before = wc.wol_list_count(b, CHAN)
        a.close()
        time.sleep(1.5)
        after = wc.wol_list_count(b, CHAN)
        return {"before": before, "after": after}
    finally:
        b.close()
        if a:
            a.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6454)
    ap.add_argument("--v3-port", type=int, default=6554)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)
        print(f"{'field':<10}{'original':<12}{'v3':<12}match")
        print("-" * 46)
        for k in ("before", "after"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<10}{ov:<12}{nv:<12}{'OK' if ov == nv else 'DIFF'}")
        print()
        if o == n and o["after"] == 1:
            print("WOL channel cleanup on disconnect matches the oracle.")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n} (v3 likely ghosts the member).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
