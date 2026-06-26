#!/usr/bin/env python3
"""Differential test: JOINCHANNEL edge cases — canonical name echo + re-join no-op.

Two behaviours beyond the happy path (diff_multichannel covers distinct channels):

  #2 EID_CHANNEL echoes the CANONICAL stored channel name (the creator's spelling),
     not the raw string this client typed. Joining "mychan" when the channel was
     created as "MyChan" must report "MyChan". (Original: channel_get_name.)
  #4 Re-joining the channel you are already in is a silent no-op — the original's
     conn_set_channel returns early ("channel == oldchannel"), sending nothing.

Run: python3 tests/diff/diff_channel_join_edges.py
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
                ev = bc.parse_chat_event(body)
                if ev:
                    out.append(ev)
    finally:
        client.sock.settimeout(old)
    return out


def _eid_channel_text(events):
    for (eid, _u, text) in events:
        if eid == EID_CHANNEL:
            return text
    return None


def scenario(host, port):
    res = {}
    alice, _ = bc.full_login(host, port, "alice", "pw")
    _join_raw(alice, "MyChan")            # creator's canonical spelling
    bob, _ = bc.full_login(host, port, "bob", "pw")
    bob_ev = _join_raw(bob, "mychan")     # different case -> same channel
    res["bob_eid_channel"] = _eid_channel_text(bob_ev)
    # bob re-joins the channel he is already in -> no-op (no events).
    res["rejoin_count"] = len(_join_raw(bob, "mychan"))
    alice.close()
    bob.close()
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
            ("canonical_name", o["bob_eid_channel"], n["bob_eid_channel"]),
            ("rejoin_noop", o["rejoin_count"], n["rejoin_count"]),
        ]
        print(f"{'field':<16}{'oracle':<14}{'v3':<14}match")
        print("-" * 52)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<16}{str(ov):<14}{str(nv):<14}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["bob_eid_channel"] == "MyChan"
                   and o["rejoin_count"] == 0)
        if success:
            print("JOINCHANNEL edges match the oracle (canonical name + re-join no-op).")
            return 0
        print("FAIL: JOINCHANNEL edge-case divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
