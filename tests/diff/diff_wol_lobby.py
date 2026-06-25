#!/usr/bin/env python3
"""Differential test: WOL post-login lobby — JOIN a channel, then LIST it.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    WOL login (CVERS/APGAR/...)         -> logged in
    JOIN #wollobby                      -> join/create the channel
    LIST                                -> the channel appears (RPL_CHANNEL 327)

and diffs the set of channel names LIST reports (normalized: leading '#' stripped,
lower-cased). A match means v3 speaks the WOL LIST dialect (327 RPL_CHANNEL with
the "<name> <count> <official> 388" layout) the way the original does, over the
shared channel repository.

Regression guard for the v3 WOL post-login LIST/JOIN wiring.

Run: python3 tests/diff/diff_wol_lobby.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

CHAN = "wollobby"


def scenario(host, port, sku=1000):
    c = wc.wol_session(host, port, "wollob", "secretpass", sku=sku)
    if c is None:
        return {"joined_listed": []}
    try:
        wc.wol_join(c, f"#{CHAN}")
        names = wc.wol_list(c)
        return {"joined_listed": names}
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
    ap.add_argument("--orig-port", type=int, default=6384)
    ap.add_argument("--v3-port", type=int, default=6484)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        # SKU 1000 (Westwood Chat) is a wolv1 client on the original.
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)
        o_has = CHAN in o["joined_listed"]
        n_has = CHAN in n["joined_listed"]
        print(f"{'field':<16}{'original':<22}{'v3':<22}match")
        print("-" * 62)
        print(f"{'lists chan':<16}{str(o_has):<22}{str(n_has):<22}"
              f"{'OK' if o_has == n_has else 'DIFF'}")
        print(f"{'list':<16}{str(o['joined_listed'])[:20]:<22}"
              f"{str(n['joined_listed'])[:20]:<22}")
        print()
        if o_has and n_has:
            print("WOL lobby matches the oracle "
                  "(joined channel appears in LIST).")
            return 0
        print("FAIL: WOL LIST divergence (joined channel not listed on both).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
