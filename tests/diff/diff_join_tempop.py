#!/usr/bin/env python3
"""Differential test: tempOP EID_INFO notice on first join of a fresh channel.

When a user joins a brand-new non-permanent channel as its first member, the
original (channel_add_connection, channel.cpp:460-471) sends, in order:

    EID_CHANNEL(0x07) -> EID_INFO(0x12) "you are now tempOP for this channel"
                      -> USERFLAGS(0x09)/SHOWUSER(0x01) roster

The EID_INFO tempOP notice is emitted precisely when the channel is not
permanent/void/clan, currmembers==1, and the account is not already a configured
operator/admin -- i.e. exactly when the joiner becomes the channel's operator by
creating it. When a user joins an ALREADY-EXISTING channel as a second member,
no tempOP notice is sent.

v3 marks the freshly-created-channel joiner as the channel operator (MF_GAVEL)
but previously omitted the EID_INFO tempOP notice, so the 0x12 event was absent.

This test drives both servers:
  - user A joins a never-before-seen channel       -> expect EID_INFO(0x12) == 1
  - user B joins that now-existing channel (2nd)   -> expect EID_INFO(0x12) == 0
and compares the structural count of EID_INFO events (the localized text is
charset-garbled in this harness, so only count/presence is compared).

Run: python3 tests/diff/diff_join_tempop.py
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


def _join_events(client, channel, collect=24, settle=0.7):
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
    # First member of a fresh channel -> tempOP.
    alice, _ = bc.full_login(host, port, "topalice", "apw")
    first = _join_events(alice, "TEMPOPFRESHCHAN")
    first_info = sum(1 for e in first if e[0] == 0x12)

    # Second member of the now-existing channel -> NOT tempOP, no notice.
    bob, _ = bc.full_login(host, port, "topbob", "bpw")
    second = _join_events(bob, "TEMPOPFRESHCHAN")
    second_info = sum(1 for e in second if e[0] == 0x12)

    # Position of the tempOP EID_INFO(0x12) relative to EID_CHANNEL(0x07) and
    # the first roster entry (USERFLAGS 0x09 / SHOWUSER 0x01). We compare only
    # this placement -- the oracle also emits extra trailing USERFLAGS from the
    # MF_PLUG/tmpOP userflags broadcast (covered by diff_join_userflags), which
    # is a separate behavior outside this test's scope.
    first_seq = [e[0] for e in first]
    info_idx = first_seq.index(0x12) if 0x12 in first_seq else None
    chan_idx = first_seq.index(0x07) if 0x07 in first_seq else None
    roster_idx = next(
        (i for i, eid in enumerate(first_seq) if eid in (0x09, 0x01)), None)
    placement = (chan_idx, info_idx, roster_idx)

    alice.close()
    bob.close()
    return {
        "fresh_info_count": first_info,
        "second_info_count": second_info,
        "fresh_placement": placement,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=13140)
    ap.add_argument("--v3-port", type=int, default=13146)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print("ORIG:", o)
        print("V3  :", n)

        ok = True
        if o["fresh_info_count"] != 1:
            print("FAIL: oracle should emit exactly 1 EID_INFO on fresh join")
            ok = False
        if n["fresh_info_count"] != o["fresh_info_count"]:
            print("DIVERGENCE: fresh-join EID_INFO count mismatch")
            ok = False
        if n["second_info_count"] != o["second_info_count"]:
            print("DIVERGENCE: second-member EID_INFO count mismatch")
            ok = False
        # EID_INFO must sit between EID_CHANNEL(0x07) and the first roster entry,
        # identically on both servers.
        if n["fresh_placement"] != o["fresh_placement"]:
            print("DIVERGENCE: fresh-join EID_INFO placement mismatch")
            ok = False
        c, i, r = o["fresh_placement"]
        if not (c is not None and i is not None and r is not None
                and c < i < r):
            print("FAIL: oracle EID_INFO not ordered CHANNEL < INFO < roster")
            ok = False

        if ok:
            print("OK: tempOP EID_INFO notice matches the oracle.")
            return 0
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
