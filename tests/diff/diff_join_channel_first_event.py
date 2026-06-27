#!/usr/bin/env python3
"""Differential test: the FIRST CHATEVENT a joiner receives is EID_CHANNEL (0x07).

On SID_JOINCHANNEL into a freshly-created channel, the original
(channel_add_connection, channel.cpp:460) sends message_type_channel =
EID_CHANNEL as the VERY FIRST CHATEVENT — before any roster (USERFLAGS/SHOWUSER)
entries. Real BNCS clients treat EID_CHANNEL as the "you are now in channel X /
reset roster" signal and expect SHOWUSER entries to follow it. v3 previously
emitted EID_CHANNEL dead last (after the self-roster), an inverted-order wire
fidelity bug.

This asserts STRUCTURE only: the event_id of the first post-JOIN CHATEVENT is
0x07 on BOTH servers (server names / statstrings / tempOP notices differ and are
not compared here — see diff_channel_join_edges.py for the canonical-name check).

Run: python3 tests/diff/diff_join_channel_first_event.py
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

EID_CHANNEL = 0x07


def _join_events(client, channel, collect=12, settle=0.6):
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
                ev = bc.parse_chat_event(body)
                if ev:
                    out.append(ev)
    finally:
        client.sock.settimeout(old)
    return out


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "pw")
    events = _join_events(alice, "ZZTOPCHAN")
    alice.close()
    first_eid = events[0][0] if events else None
    return {"first_eid": first_eid, "count": len(events)}


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
            ("first_event_id",
             None if o["first_eid"] is None else f"0x{o['first_eid']:02x}",
             None if n["first_eid"] is None else f"0x{n['first_eid']:02x}"),
        ]
        print(f"{'field':<18}{'oracle':<12}{'v3':<12}match")
        print("-" * 50)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<18}{str(ov):<12}{str(nv):<12}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["first_eid"] == EID_CHANNEL)
        if success:
            print("JOINCHANNEL first CHATEVENT is EID_CHANNEL (0x07) on both servers.")
            return 0
        print("FAIL: JOINCHANNEL first-event ordering divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
