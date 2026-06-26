#!/usr/bin/env python3
"""Differential test: WOL STARTG game-start relay (the game-lobby capstone).

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client A: JOINGAME #startg ... (CREATE)
    client B: JOINGAME #startg     (JOIN)
    client A: STARTG #startg B
    client B receives ":A!.. STARTG B :<owner_ip> <gameNumber> <time_t>"

and diffs whether the named player receives a STARTG carrying the owner's IP
(127.0.0.1 for loopback clients). NOTE: the trailing gameNumber/time_t differ per
server/run, so this is a TOLERANT check (delivery + owner-IP presence), not a
byte-exact comparison — STARTG is the one WOL command whose payload cannot be
byte-diffed.

Run: python3 tests/diff/diff_wol_startg.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

GAME = "startg"


def scenario(host, port, sku=1000):
    a = wc.wol_session(host, port, "startga", "secretpass", sku=sku)
    b = wc.wol_session(host, port, "startgb", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"delivered": None, "has_owner_ip": None}
    try:
        wc.wol_joingame_create(a, f"#{GAME}", 2, 8, 1, 0)
        wc.wol_read_after_verb(a, "JOINGAME")
        wc.wol_joingame_join(b, f"#{GAME}")
        wc.wol_read_after_verb(b, "JOINGAME")
        wc.wol_startg(a, f"#{GAME}", "startgb")
        got = wc.wol_read_after_verb(b, "STARTG")
        if got is None:
            return {"delivered": False, "has_owner_ip": False}
        _sender, payload = got
        return {"delivered": True, "has_owner_ip": "127.0.0.1" in payload}
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
    ap.add_argument("--orig-port", type=int, default=6428)
    ap.add_argument("--v3-port", type=int, default=6528)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<14}{'original':<12}{'v3':<12}match")
        print("-" * 48)
        ok = True
        for k in ("delivered", "has_owner_ip"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<14}{ov:<12}{nv:<12}{'OK' if m else 'DIFF'}")
        print()

        if ok and o["delivered"] is True and o["has_owner_ip"] is True:
            print("WOL STARTG matches the oracle "
                  "(named player receives STARTG with the owner IP).")
            return 0
        print("FAIL: WOL STARTG divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
