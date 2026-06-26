#!/usr/bin/env python3
"""Differential test: /squelch is per-connection — cleared on disconnect.

The original's ignore list lives on the connection (conn_destroy frees it), so a
squelch must NOT survive a disconnect/reconnect: a fresh connection starts with
an empty ignore list. v3's ignore store is account-keyed and run-loop-scoped, so
without explicit cleanup a squelch would persist across reconnect — diverging
from the oracle.

    alice + bob join channel
    alice: /squelch bob
    bob talks            -> alice (still squelching) does NOT hear it (control)
    alice disconnects, reconnects, rejoins
    bob talks            -> alice MUST hear it again (squelch cleared on dc)

Run: python3 tests/diff/diff_squelch_reconnect.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "SqChan"
EID_TALK = 0x05


def _heard(events, text):
    return any(eid == EID_TALK and text in t for (eid, _u, t) in events)


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bob, _ = bc.full_login(host, port, "bob", "bobpass")
    bc.join_channel(alice, CHAN)
    bc.join_channel(bob, CHAN)
    bc.drain_chat(alice)

    bc.chat_command(alice, "/squelch bob")
    bc.drain_chat(alice)
    bob.send(bc.SID_CHATCOMMAND, bc.cstring("muted"))
    while_squelched = _heard(bc.drain_chat(alice), "muted")

    # alice reconnects on a fresh connection (same account).
    alice.close()
    time.sleep(0.6)
    alice2, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.join_channel(alice2, CHAN)
    bc.drain_chat(alice2)
    bob.send(bc.SID_CHATCOMMAND, bc.cstring("reconn"))
    after_reconnect = _heard(bc.drain_chat(alice2), "reconn")

    alice2.close()
    bob.close()
    return {"while_squelched": while_squelched, "after_reconnect": after_reconnect}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6380)
    ap.add_argument("--v3-port", type=int, default=6480)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        rows = [
            ("squelched_mute", o["while_squelched"], n["while_squelched"]),
            ("cleared_on_dc", o["after_reconnect"], n["after_reconnect"]),
        ]
        print(f"{'field':<16}{'original':<12}{'v3':<12}match")
        print("-" * 52)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<16}{str(ov):<12}{str(nv):<12}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["while_squelched"] is False
                   and o["after_reconnect"] is True)
        if success:
            print("Squelch is per-connection — cleared on disconnect (matches oracle).")
            return 0
        print("FAIL: squelch-persistence-across-reconnect divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
