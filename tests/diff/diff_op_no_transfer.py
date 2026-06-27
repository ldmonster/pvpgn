#!/usr/bin/env python3
"""Differential test: the channel operator (gavel) is NOT migrated on leave.

The original makes the FIRST user to join a fresh non-permanent channel its
temporary operator (tmpOP / MF_GAVEL 0x02). When that operator leaves,
disconnects, or is kicked, the original does NOT promote a remaining member --
it only clears the departing user's tmpOP (channel.cpp: on member removal it
calls conn_set_tmpOP_channel(connection, NULL) and promotes no one).

v3 previously migrated the gavel to a remaining member (wave 58), so a normal
member silently inherited operator status. This guards against that regression.

Decisive observable (masked to the OP bit 0x02; the original also toggles a
transient MF_PLUG 0x10 UDP-capability bit that v3 does not model):

    alice joins fresh channel "OpHunt"  -> she is the operator (0x02)
    bob   joins "OpHunt"                 -> normal user (0)
    alice disconnects                    -> operator slot cleared, NOT migrated
    carol joins "OpHunt"                 -> sees bob with OP bit 0 (no gavel)

Run: python3 tests/diff/diff_op_no_transfer.py
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

EID_SHOWUSER = 0x01
OP_BIT = 0x02
CHAN = "OpHunt"


def _parse(body):
    if body is None or len(body) < 24:
        return None
    eid = struct.unpack_from("<I", body, 0)[0]
    flags = struct.unpack_from("<I", body, 4)[0]
    user = body[24:].split(b"\x00")[0].decode("latin-1", "replace")
    return eid, flags, user


def _join_raw(client, channel, collect=12, settle=0.6):
    client.send(bc.SID_JOINCHANNEL, struct.pack("<I", 0) + bc.cstring(channel))
    time.sleep(settle)
    out = []
    old = client.sock.gettimeout()
    client.sock.settimeout(0.4)
    try:
        for _ in range(collect):
            r = client.recv()
            if r is None:
                break
            sid, body = r
            if sid == bc.SID_PING:
                client.send(bc.SID_PING, body[:4])
                continue
            if sid == bc.SID_CHATEVENT:
                ev = _parse(body)
                if ev:
                    out.append(ev)
    finally:
        client.sock.settimeout(old)
    return out


def _op_bit_for(events, user):
    """OR of the OP bit across this user's SHOWUSER events (1 if ever operator)."""
    seen = False
    bit = 0
    for (eid, flags, u) in events:
        if eid == EID_SHOWUSER and u.lower() == user.lower():
            seen = True
            bit |= (flags & OP_BIT)
    return (seen, bit)


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "pw")
    _join_raw(alice, CHAN)          # alice is first -> operator (tmpOP)
    bob, _ = bc.full_login(host, port, "bob", "pw")
    _join_raw(bob, CHAN)            # bob -> normal user
    time.sleep(0.4)
    alice.close()                   # the operator drops
    time.sleep(0.8)
    carol, _ = bc.full_login(host, port, "carol", "pw")
    carol_events = _join_raw(carol, CHAN)
    res = {
        "carol_sees_bob_op": _op_bit_for(carol_events, "bob"),
    }
    bob.close()
    carol.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6390)
    ap.add_argument("--v3-port", type=int, default=6490)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        rows = [
            ("bob_not_op_after_op_left", o["carol_sees_bob_op"],
             n["carol_sees_bob_op"]),
        ]
        print(f"{'field':<26}{'oracle':<18}{'v3':<18}match")
        print("-" * 68)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<26}{str(ov):<18}{str(nv):<18}{'OK' if same else 'DIFF'}")
        print()
        # Decisive: the oracle leaves bob WITHOUT the gavel after the op left.
        success = (all_ok and o["carol_sees_bob_op"][1] == 0)
        if success:
            print("Operator (gavel) is not migrated on leave -- matches the oracle.")
            return 0
        print("FAIL: gavel wrongly migrated to a remaining member.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
