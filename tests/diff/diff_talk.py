#!/usr/bin/env python3
"""Differential test: two-client channel talk delivery.

Alice and Bob both join the same channel; Alice sends a SID_CHATCOMMAND.
The original server delivers an EID_TALK chat event to Bob (everyone in the
channel except the speaker). v3 must do the same.

Regression guard for bug-hunt wave 14: v3's broadcast_chat_event double-wrapped
the SID_CHATEVENT packet (encode() already begins+finalizes), so the redundant
finalize failed and every channel broadcast was silently dropped — Bob received
nothing.

Run: python3 tests/diff/diff_talk.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def talk_scenario(host, port):
    """Returns the list of (eid, user, text) Bob receives after Alice talks."""
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "PvPGN")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.join_channel(b, "PvPGN")
    time.sleep(0.3)
    while a.recv() is not None:
        pass
    while b.recv() is not None:
        pass
    a.send(bc.SID_CHATCOMMAND, bc.cstring("hi bob"))
    time.sleep(0.4)
    out = []
    for _ in range(6):
        r = b.recv()
        if r and r[0] == bc.SID_CHATEVENT:
            ev = bc.parse_chat_event(r[1])
            if ev:
                out.append((ev[0], ev[1], ev[2]))
    a.close()
    b.close()
    # Keep only TALK events (eid 0x05) for a stable comparison.
    return [e for e in out if e[0] == 0x05]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6322)
    ap.add_argument("--v3-port", type=int, default=6422)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = talk_scenario("127.0.0.1", args.orig_port)
        n = talk_scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: Bob got {o}")
        print(f"V3      : Bob got {n}")
        if o == n and len(n) == 1:
            print("PASS: channel talk delivery matches oracle")
            return 0
        print("FAIL: divergence in channel talk delivery")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
