#!/usr/bin/env python3
"""Differential test: WOL INVMSG game-invite relay.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client B: WOL login
    client A: WOL login
    client A: INVMSG #invroom 1 invb   -> client B receives ":..!.. INVMSG #invroom 1"

and diffs whether the invited online user receives the INVMSG relay (and the
channel token it carries). A match means v3 relays WOL game invites to the named
users the way the original does.

Run: python3 tests/diff/diff_wol_invmsg.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

ROOM = "invroom"


def scenario(host, port, sku=1000):
    b = wc.wol_session(host, port, "invb", "secretpass", sku=sku)
    a = wc.wol_session(host, port, "inva", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"delivered": None, "payload": None}
    try:
        wc.wol_invmsg(a, f"#{ROOM}", "1", "invb")
        got = wc.wol_read_after_verb(b, "INVMSG")
        if got is None:
            return {"delivered": False, "payload": None}
        _sender, payload = got
        return {"delivered": True, "payload": payload.strip()}
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
    ap.add_argument("--orig-port", type=int, default=6426)
    ap.add_argument("--v3-port", type=int, default=6526)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<12}{'original':<22}{'v3':<22}match")
        print("-" * 62)
        ok = True
        for k in ("delivered", "payload"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<12}{ov:<22}{nv:<22}{'OK' if m else 'DIFF'}")
        print()

        if (ok and o["delivered"] is True and o["payload"] == f"invb #{ROOM} 1"):
            print("WOL INVMSG matches the oracle (invited user receives the relay).")
            return 0
        print("FAIL: WOL INVMSG divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
