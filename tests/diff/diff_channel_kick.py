#!/usr/bin/env python3
"""Differential test: /kick (channel operator removes a member).

The original lets a channel operator (gavel/tmpOP) kick a member: the target is
removed from the channel, the remaining members receive EID_LEAVE for the target,
and the target is notified. A NON-operator's /kick is refused (target stays).

The oracle sends a few extra cosmetic events (an EID_INFO ack to the operator, an
EID_CHANNEL move + localized error to the victim); this compares the DECISIVE
observables: (a) op-kick makes the remaining members see the target leave, and
(b) a non-operator's kick has no effect.

Run: python3 tests/diff/diff_channel_kick.py
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

EID_LEAVE = 0x03


def _parse(body):
    if body is None or len(body) < 24:
        return None
    eid = struct.unpack_from("<I", body, 0)[0]
    user = body[24:].split(b"\x00")[0].decode("latin-1", "replace")
    return eid, user


def _drain(client, settle=0.6, collect=20):
    time.sleep(settle)
    old = client.sock.gettimeout()
    client.sock.settimeout(0.4)
    out = []
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


def _join(client, channel):
    client.send(bc.SID_JOINCHANNEL, struct.pack("<I", 0) + bc.cstring(channel))
    return _drain(client)


def _cmd(client, text):
    client.send(bc.SID_CHATCOMMAND, bc.cstring(text))
    return _drain(client)


def _saw_leave(events, user):
    return any(eid == EID_LEAVE and u.lower() == user.lower()
               for (eid, u) in events)


def scenario(host, port):
    res = {}
    # S1: alice (operator/first user) kicks bob.
    alice, _ = bc.full_login(host, port, "alice", "pw")
    _join(alice, "KickCh")
    bob, _ = bc.full_login(host, port, "bob", "pw")
    _join(bob, "KickCh")
    _drain(alice)
    _drain(bob)
    alice_stream = _cmd(alice, "/kick bob")
    res["op_kick_member_leaves"] = _saw_leave(alice_stream, "bob")

    # S2: non-operator (dave) tries to kick the operator (carol) -> refused.
    carol, _ = bc.full_login(host, port, "carol", "pw")
    _join(carol, "KickCh2")  # carol is operator of KickCh2
    dave, _ = bc.full_login(host, port, "dave", "pw")
    _join(dave, "KickCh2")
    _drain(carol)
    dave_stream = _cmd(dave, "/kick carol")
    # carol must NOT have left (dave is not operator).
    res["nonop_kick_no_effect"] = not _saw_leave(dave_stream, "carol")

    for c in (alice, bob, carol, dave):
        c.close()
    return res


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
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        rows = [
            ("op_kick_leaves", o["op_kick_member_leaves"], n["op_kick_member_leaves"]),
            ("nonop_no_effect", o["nonop_kick_no_effect"], n["nonop_kick_no_effect"]),
        ]
        print(f"{'field':<18}{'oracle':<10}{'v3':<10}match")
        print("-" * 48)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<18}{str(ov):<10}{str(nv):<10}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["op_kick_member_leaves"]
                   and o["nonop_kick_no_effect"])
        if success:
            print("/kick matches the oracle (operator removes member; non-op refused).")
            return 0
        print("FAIL: /kick divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
