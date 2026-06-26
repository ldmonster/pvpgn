#!/usr/bin/env python3
"""Differential test: WOL SETCODEPAGE/GETCODEPAGE, SETLOCALE/GETLOCALE, GETINSIDER.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener), for one
logged-in client:

    SETCODEPAGE 1252  -> 329 "1252"
    GETCODEPAGE <me>  -> 328 "<me>`1252"
    SETLOCALE 5       -> 310 "5"
    GETLOCALE <me>    -> 309 "<me>`5"
    GETINSIDER <me>   -> 399 "<me>`0"

and diffs each reply payload. A match means v3 reproduces the WOL codepage/locale
get-set echoes and the GETINSIDER reply the way the original does. (Self-query;
cross-session codepage/locale lookup of OTHER users is a documented v3
simplification and not exercised here.)

Run: python3 tests/diff/diff_wol_userinfo.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

ME = "cpuser"


def scenario(host, port, sku=1000):
    c = wc.wol_session(host, port, ME, "secretpass", sku=sku)
    if c is None:
        return {k: None for k in
                ("setcp", "getcp", "setloc", "getloc", "insider")}
    try:
        return {
            "setcp":  wc.wol_cmd_reply(c, "SETCODEPAGE 1252", 329),
            "getcp":  wc.wol_cmd_reply(c, f"GETCODEPAGE {ME}", 328),
            "setloc": wc.wol_cmd_reply(c, "SETLOCALE 5", 310),
            "getloc": wc.wol_cmd_reply(c, f"GETLOCALE {ME}", 309),
            "insider": wc.wol_cmd_reply(c, f"GETINSIDER {ME}", 399),
        }
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
    ap.add_argument("--orig-port", type=int, default=6398)
    ap.add_argument("--v3-port", type=int, default=6498)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<10}{'original':<18}{'v3':<18}match")
        print("-" * 52)
        ok = True
        for k in ("setcp", "getcp", "setloc", "getloc", "insider"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<10}{ov:<18}{nv:<18}{'OK' if m else 'DIFF'}")
        print()

        want = {"setcp": "1252", "getcp": f"{ME}`1252", "setloc": "5",
                "getloc": f"{ME}`5", "insider": f"{ME}`0"}
        if ok and all(o[k] == want[k] for k in want):
            print("WOL codepage/locale/insider match the oracle.")
            return 0
        print("FAIL: WOL codepage/locale/insider divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
