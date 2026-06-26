#!/usr/bin/env python3
"""Differential test: WOL USERIP user-address lookup.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client B: WOL login
    client A: WOL login
    client A: USERIP useripb   -> ":..!.. USERIP useripb <ip>"   (online)
    client A: USERIP ghost     -> 401 ERR_NOSUCHNICK             (unknown)

and diffs the reported IP for an online user (both clients connect from
127.0.0.1, so both servers report the loopback address) and the 401 for an
unknown user. A match means v3's peer-address registry reports a user's IP the
way the original does — the infra that also unblocks STARTG.

Run: python3 tests/diff/diff_wol_userip.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def scenario(host, port, sku=1000):
    b = wc.wol_session(host, port, "useripb", "secretpass", sku=sku)
    a = wc.wol_session(host, port, "useripa", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"online_status": None, "online_ip": None, "unknown_status": None}
    try:
        st, ip = wc.wol_userip(a, "useripb")
        ust, _ = wc.wol_userip(a, "ghostuserip_nope")
        return {"online_status": st, "online_ip": ip, "unknown_status": ust}
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
    ap.add_argument("--orig-port", type=int, default=6424)
    ap.add_argument("--v3-port", type=int, default=6524)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<16}{'original':<16}{'v3':<16}match")
        print("-" * 58)
        ok = True
        for k in ("online_status", "online_ip", "unknown_status"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<16}{ov:<16}{nv:<16}{'OK' if m else 'DIFF'}")
        print()

        if (ok and o["online_status"] == "ok" and o["online_ip"] == "127.0.0.1"
                and o["unknown_status"] == "401"):
            print("WOL USERIP matches the oracle (reports IP online, 401 unknown).")
            return 0
        print("FAIL: WOL USERIP divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
