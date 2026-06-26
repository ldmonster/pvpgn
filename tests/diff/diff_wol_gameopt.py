#!/usr/bin/env python3
"""Differential test: WOL GAMEOPT channel game-option relay.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client A: WOL login -> JOIN #wolopt
    client B: WOL login -> JOIN #wolopt
    client A: GAMEOPT #wolopt :speed=6 color=red units=10
    client B: receives ":A!WWOL@.. GAMEOPT #wolopt :speed=6 color=red units=10"

and diffs whether the OTHER channel member receives the opaque game-options text.
A match means v3 relays GAMEOPT to channel members the way the original does
(channel_message_send / message_type_gameopt_talk) — part of the WOL game lobby.

Run: python3 tests/diff/diff_wol_gameopt.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

CHAN = "wolopt"
OPTS = "speed=6 color=red units=10"


def scenario(host, port, sku=1000):
    """Returns {received: bool, text: str|None}."""
    a = wc.wol_session(host, port, "optspeaker", "secretpass", sku=sku)
    b = wc.wol_session(host, port, "optlistener", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"received": False, "text": None}
    try:
        wc.wol_join(a, f"#{CHAN}")
        wc.wol_join(b, f"#{CHAN}")
        wc.wol_gameopt(a, f"#{CHAN}", OPTS)
        got = wc.wol_read_verb(b, "GAMEOPT")
        if got is None:
            return {"received": False, "text": None}
        _sender, _target, text = got
        return {"received": True, "text": text}
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
    ap.add_argument("--orig-port", type=int, default=6388)
    ap.add_argument("--v3-port", type=int, default=6488)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<14}{'original':<28}{'v3':<28}match")
        print("-" * 72)
        for k in ("received", "text"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<14}{ov:<28}{nv:<28}{'OK' if ov == nv else 'DIFF'}")
        print()

        if o["received"] and n["received"] and o["text"] == n["text"] == OPTS:
            print("WOL GAMEOPT matches the oracle "
                  "(other member receives the game options).")
            return 0
        print("FAIL: WOL GAMEOPT divergence "
              "(options not delivered to the other member on both).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
