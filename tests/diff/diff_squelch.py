#!/usr/bin/env python3
"""Differential test: /squelch — ignored users' channel messages are suppressed.

Drives, against BOTH the original pvpgn-server and v3:

    alice + bob join channel "TestChan"
    bob: "before"            -> alice receives bob's EID_TALK   (control)
    alice: /squelch bob
    bob: "after"             -> alice does NOT receive bob's EID_TALK

The decisive observable is message DELIVERY (EID_TALK), not the squelch reply
text — which the original localize()s + charset-converts (garbled in this
harness, see diff_channelcmds.py). A match means v3 enforces the squelch on the
broadcast side exactly like the original's MF_X "player is ignored" filter.

Regression guard for the v3 /squelch handler + broadcast-side ignore filtering.

Run: python3 tests/diff/diff_squelch.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "TestChan"
EID_TALK = 0x05


def _bob_said(events, text):
    return any(eid == EID_TALK and text in t for (eid, _u, t) in events)


def scenario(host, port):
    out = {}
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bob, _ = bc.full_login(host, port, "bob", "bobpass")
    bc.join_channel(alice, CHAN)
    bc.join_channel(bob, CHAN)
    bc.drain_chat(alice)  # clear join noise

    # Control: bob talks, alice should hear it.
    bob.send(bc.SID_CHATCOMMAND, bc.cstring("before"))
    out["before_heard"] = _bob_said(bc.drain_chat(alice), "before")

    # alice squelches bob.
    bc.chat_command(alice, "/squelch bob")
    bc.drain_chat(alice)

    # After squelch: bob talks, alice should NOT hear it.
    bob.send(bc.SID_CHATCOMMAND, bc.cstring("after"))
    out["after_heard"] = _bob_said(bc.drain_chat(alice), "after")

    alice.close()
    bob.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6382)
    ap.add_argument("--v3-port", type=int, default=6482)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        keys = ["before_heard", "after_heard"]
        print(f"{'field':<16}{'original':<12}{'v3':<12}match")
        print("-" * 48)
        all_ok = True
        for k in keys:
            same = o.get(k) == n.get(k)
            all_ok &= same
            print(f"{k:<16}{str(o.get(k)):<12}{str(n.get(k)):<12}"
                  f"{'OK' if same else 'DIFF'}")
        success = (all_ok and o.get("before_heard") is True
                   and o.get("after_heard") is False)
        print()
        if success:
            print("/squelch matches the oracle "
                  "(message heard before, suppressed after).")
            return 0
        print("FAIL: divergence in /squelch suppression.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
