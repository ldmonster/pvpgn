#!/usr/bin/env python3
"""Differential test: WOL SETOPT (findme/pageme gating of FINDUSER/PAGE).

SETOPT has no direct reply; its effect is observable only through FINDUSER and
PAGE, which consult the TARGET user's flags. Drives, against BOTH the original
pvpgn-server and v3:

    B,A login
    A FINDUSER B / PAGE B            -> 0 / 0   (defaults: findable + pageable)
    B SETOPT 16,32                   (find off, page off)
    A FINDUSER B / PAGE B            -> 1 / 1   (disabled)
    B SETOPT 17,33                   (find on, page on)
    A FINDUSER B / PAGE B            -> 0 / 0   (re-enabled)

and diffs each status. A match means v3 honours SETOPT's findme/pageme toggles
across sessions the way the original does.

Run: python3 tests/diff/diff_wol_setopt.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def scenario(host, port, sku=1000):
    b = wc.wol_session(host, port, "setoptb", "secretpass", sku=sku)
    a = wc.wol_session(host, port, "setopta", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {k: None for k in
                ("find0", "page0", "find_off", "page_off", "find_on", "page_on")}
    try:
        def find(nick):
            wc.wol_finduser(a, nick)
            return wc.wol_read_finduser(a)

        r = {}
        r["find0"] = find("setoptb")
        r["page0"] = wc.wol_page(a, "setoptb", "x")
        wc.wol_setopt(b, "16,32")  # find off, page off
        r["find_off"] = find("setoptb")
        r["page_off"] = wc.wol_page(a, "setoptb", "x")
        wc.wol_setopt(b, "17,33")  # find on, page on
        r["find_on"] = find("setoptb")
        r["page_on"] = wc.wol_page(a, "setoptb", "x")
        return r
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
    ap.add_argument("--orig-port", type=int, default=6430)
    ap.add_argument("--v3-port", type=int, default=6530)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        fields = ("find0", "page0", "find_off", "page_off", "find_on", "page_on")
        print(f"{'field':<12}{'original':<12}{'v3':<12}match")
        print("-" * 46)
        ok = True
        for k in fields:
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<12}{ov:<12}{nv:<12}{'OK' if m else 'DIFF'}")
        print()

        want = {"find0": "0", "page0": "0", "find_off": "1", "page_off": "1",
                "find_on": "0", "page_on": "0"}
        if ok and all(o[k] == want[k] for k in fields):
            print("WOL SETOPT matches the oracle (find/page toggles honoured).")
            return 0
        print("FAIL: WOL SETOPT divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
