#!/usr/bin/env python3
"""Differential test: WOL FINDUSER / FINDUSEREX user-presence lookup.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client B: WOL login -> JOIN #findroom
    client A: WOL login
    client A: FINDUSER B          -> "0 :<channel>"  (online, findme defaults on)
    client A: FINDUSER ghostuser  -> "1 :"           (not found)
    client A: FINDUSEREX B        -> "0 :<channel>,0"

and diffs the status field ('0' found / '1' not) for an online user and an
unknown user, for both FINDUSER (388) and FINDUSEREX (398). A match means v3
reports WOL user presence the way the original does. (The channel-name payload
itself is not byte-compared — irc_convert_channel formatting is a known harness
divergence; the presence status is the behavioural contract.)

Run: python3 tests/diff/diff_wol_finduser.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

ROOM = "findroom"


def scenario(host, port, sku=1000):
    """Returns {found, notfound, found_ex} status chars ('0'/'1'/None)."""
    b = wc.wol_session(host, port, "finduserb", "secretpass", sku=sku)
    a = wc.wol_session(host, port, "findusera", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"found": None, "notfound": None, "found_ex": None}
    try:
        wc.wol_join(b, f"#{ROOM}")
        wc.wol_finduser(a, "finduserb")
        found = wc.wol_read_finduser(a)
        wc.wol_finduser(a, "ghostuser_nope")
        notfound = wc.wol_read_finduser(a)
        wc.wol_finduser(a, "finduserb", ex=True)
        found_ex = wc.wol_read_finduser(a, ex=True)
        return {"found": found, "notfound": notfound, "found_ex": found_ex}
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
    ap.add_argument("--orig-port", type=int, default=6394)
    ap.add_argument("--v3-port", type=int, default=6494)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<12}{'original':<14}{'v3':<14}match")
        print("-" * 50)
        ok = True
        for k in ("found", "notfound", "found_ex"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<12}{ov:<14}{nv:<14}{'OK' if m else 'DIFF'}")
        print()

        if ok and o["found"] == "0" and o["notfound"] == "1":
            print("WOL FINDUSER matches the oracle (online found, unknown not).")
            return 0
        print("FAIL: WOL FINDUSER divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
