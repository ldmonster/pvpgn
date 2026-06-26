#!/usr/bin/env python3
"""Differential test: WOL JOINGAME create + join (game-as-channel model).

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client A: WOL login -> JOINGAME #wolgame 2 8 1 1 1 0   (CREATE: min max type .. tourn)
    client B: WOL login -> JOINGAME #wolgame 1             (JOIN)
    client B (and A) receive ":..!.. JOINGAME 2 8 1 1 1 0 :#wolgame"

and diffs the numeric prefix of the JOINGAME join-ack (min max type 1 1 tourn)
the joiner receives. A match means v3 reproduces the WOL game create/join flow —
a game hosted on one connection is found and joined from another, with the WOLv1
acknowledgement layout — the way the original does.

Run: python3 tests/diff/diff_wol_joingame.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

GAME = "wolgame"
MIN, MAX, GTYPE, TOURN = 2, 8, 1, 0


def numeric_prefix(rest):
    """Return the fields of a JOINGAME ack before the ' :' channel trailer."""
    if rest is None:
        return None
    head = rest.split(" :", 1)[0]
    return head.strip()


def scenario(host, port, sku=1000):
    """Returns {created: bool, joined: bool, join_ack: str|None}."""
    a = wc.wol_session(host, port, "gamehost", "secretpass", sku=sku)
    b = wc.wol_session(host, port, "gamejoiner", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"created": False, "joined": False, "join_ack": None}
    try:
        wc.wol_joingame_create(a, f"#{GAME}", MIN, MAX, GTYPE, TOURN)
        a_ack = wc.wol_read_after_verb(a, "JOINGAME")
        wc.wol_joingame_join(b, f"#{GAME}")
        b_ack = wc.wol_read_after_verb(b, "JOINGAME")
        return {
            "created": a_ack is not None,
            "joined": b_ack is not None,
            "join_ack": numeric_prefix(b_ack[1]) if b_ack else None,
        }
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
    ap.add_argument("--orig-port", type=int, default=6392)
    ap.add_argument("--v3-port", type=int, default=6492)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<12}{'original':<22}{'v3':<22}match")
        print("-" * 60)
        for k in ("created", "joined", "join_ack"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<12}{ov:<22}{nv:<22}{'OK' if ov == nv else 'DIFF'}")
        print()

        want = f"{MIN} {MAX} {GTYPE} 1 1 {TOURN}"
        if (o["created"] and o["joined"] and n["created"] and n["joined"]
                and o["join_ack"] == n["join_ack"] == want):
            print("WOL JOINGAME matches the oracle "
                  "(game created, found and joined; same ack layout).")
            return 0
        print(f"FAIL: WOL JOINGAME divergence (want join ack prefix '{want}').")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
