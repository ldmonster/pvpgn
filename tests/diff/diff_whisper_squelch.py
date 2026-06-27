#!/usr/bin/env python3
"""Differential test: /squelch suppresses a squelched sender's WHISPER too.

Drives, against BOTH the original pvpgn-server and v3:

    alice + bob join a channel
    bob: /squelch alice          (bob ignores alice)
    alice: /w bob hello_there

The original's message_send sets MF_X for any dst that is ignoring src, and
message_type_whisper returns -1 — so the recipient (bob) gets NO EID_WHISPER,
while the sender (alice) still gets her own EID_WHISPERSENT ack (unaffected by
the target's ignore list). v3 previously delivered the whisper unconditionally.

Decisive observables (compared structurally, EIDs only — reply text is
localize()+charset-garbled in this harness):
  - bob_whisper_recv: EID_WHISPER (0x04) text bob receives  -> [] on both
  - alice_whispersent: EID_WHISPERSENT (0x0a) text alice receives -> ['hello_there']

Regression guard for whisper-side ignore filtering (bug-hunt wave: whisper
squelch parity).

Run: python3 tests/diff/diff_whisper_squelch.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "WhSq"
EID_WHISPER = 0x04
EID_WHISPERSENT = 0x0a


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bob, _ = bc.full_login(host, port, "bob", "bobpass")
    bc.join_channel(alice, CHAN)
    bc.join_channel(bob, CHAN)
    bc.drain_chat(alice)
    bc.drain_chat(bob)

    # bob squelches alice.
    bc.chat_command(bob, "/squelch alice")
    bc.drain_chat(bob)
    bc.drain_chat(alice)

    # alice whispers bob — should be dropped for bob, ack'd to alice.
    alice_events = bc.chat_command(alice, "/w bob hello_there")
    time.sleep(0.4)
    bob_events = bc.drain_chat(bob)

    alice.close()
    bob.close()
    return {
        "alice_whispersent": [t for (eid, _u, t) in alice_events
                              if eid == EID_WHISPERSENT],
        "bob_whisper_recv": [t for (eid, _u, t) in bob_events
                             if eid == EID_WHISPER],
    }


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
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
    finally:
        v3.stop()
        orig.stop()

    keys = ["alice_whispersent", "bob_whisper_recv"]
    print(f"{'field':<20}{'original':<16}{'v3':<16}match")
    print("-" * 60)
    all_ok = True
    for k in keys:
        same = o.get(k) == n.get(k)
        all_ok &= same
        print(f"{k:<20}{str(o.get(k)):<16}{str(n.get(k)):<16}"
              f"{'OK' if same else 'DIFF'}")
    # Sanity: oracle must actually exhibit the suppression we are matching.
    oracle_ok = (o.get("bob_whisper_recv") == []
                 and o.get("alice_whispersent") == ["hello_there"])
    print()
    if all_ok and oracle_ok:
        print("whisper /squelch matches the oracle "
              "(recipient suppressed, sender ack preserved).")
        return 0
    print("FAIL: divergence in whisper squelch suppression.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
