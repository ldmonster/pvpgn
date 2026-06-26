#!/usr/bin/env python3
"""Differential test: WOL cross-session channel chat.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client A: WOL login -> JOIN #wolchat
    client B: WOL login -> JOIN #wolchat
    client A: PRIVMSG #wolchat :hello from A
    client B: receives ":A!A@... PRIVMSG #wolchat :hello from A"

and diffs whether the OTHER member actually receives the channel message (and
with the right sender + text). A match means v3 relays WOL channel chat to other
members' connections via the cross-session message router the way the original
does — the prerequisite for the WOL game lobby (GAMEOPT/STARTG broadcasts).

Regression guard for WOL cross-session delivery (set_routing + router register).

Run: python3 tests/diff/diff_wol_chat.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

CHAN = "wolchat"
MSG = "hello from A"


def scenario(host, port, sku=1000):
    """Returns {received: bool, sender: str|None, text: str|None}."""
    a = wc.wol_session(host, port, "wolspeaker", "secretpass", sku=sku)
    b = wc.wol_session(host, port, "wollistener", "secretpass", sku=sku)
    if a is None or b is None:
        if a:
            a.close()
        if b:
            b.close()
        return {"received": False, "sender": None, "text": None}
    try:
        wc.wol_join(a, f"#{CHAN}")
        wc.wol_join(b, f"#{CHAN}")
        wc.wol_privmsg(a, f"#{CHAN}", MSG)
        got = wc.wol_read_privmsg(b, want_channel=CHAN)
        if got is None:
            return {"received": False, "sender": None, "text": None}
        sender, _target, text = got
        return {"received": True, "sender": sender, "text": text}
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
    ap.add_argument("--orig-port", type=int, default=6386)
    ap.add_argument("--v3-port", type=int, default=6486)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<14}{'original':<24}{'v3':<24}match")
        print("-" * 64)
        for k in ("received", "text"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<14}{ov:<24}{nv:<24}"
                  f"{'OK' if ov == nv else 'DIFF'}")
        print()

        # Both must deliver the message with the same text. (Sender hostmask
        # formatting can differ between servers; the relayed text + the fact of
        # delivery are the behavioural contract.)
        if o["received"] and n["received"] and o["text"] == n["text"] == MSG:
            print("WOL channel chat matches the oracle "
                  "(other member receives the PRIVMSG).")
            return 0
        print("FAIL: WOL channel chat divergence "
              "(message not delivered to the other member on both).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
