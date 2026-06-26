#!/usr/bin/env python3
"""Differential test: a KICKED session (kick-old-login) leaves its channel.

Lifecycle bug-hunt follow-on to w49/w50 (kick-old-login). When alice logs in a
second time, the first session is kicked. The question this checks: does the
kicked session's channel membership get cleaned up so other members in that
channel observe alice leaving (EID_LEAVE), or does the kicked session ghost in
the roster?

    bob:    login, JOIN "W"
    alice1: login, JOIN "W"        (bob sees alice EID_JOIN)
    alice2: login (same account)   -> kicks alice1
    bob:    must observe EID_LEAVE for alice (kicked session left the channel)

Compares the set of (event_id, user) bob receives after the kick against the
oracle. Run: python3 tests/diff/diff_kick_channel.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

EID_LEAVE = 0x03


def scenario(host, port):
    bob, _ = bc.full_login(host, port, "bob", "bobpass")
    bc.join_channel(bob, "W")
    alice1, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.join_channel(alice1, "W")
    time.sleep(0.3)
    bc.drain_chat(bob)  # flush alice1's EID_JOIN

    # alice logs in a second time from a fresh connection -> kicks alice1.
    alice2, _ = bc.full_login(host, port, "alice", "alicepass")
    time.sleep(0.6)

    events = bc.drain_chat(bob)
    leaves = sorted({u for (eid, u, _t) in events if eid == EID_LEAVE})

    alice1.close()
    alice2.close()
    bob.close()
    return {"leaves": leaves}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6376)
    ap.add_argument("--v3-port", type=int, default=6476)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        alice_left_o = any("alice" in u.lower() for u in o["leaves"])
        alice_left_n = any("alice" in u.lower() for u in n["leaves"])
        rows = [
            ("alice_left", alice_left_o, alice_left_n),
            ("leaves", o["leaves"], n["leaves"]),
        ]
        print(f"{'field':<14}{'original':<22}{'v3':<22}match")
        print("-" * 64)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<14}{str(ov):<22}{str(nv):<22}{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("Kicked session leaves its channel (matches oracle).")
            return 0
        print("FAIL: kicked-session channel cleanup divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
