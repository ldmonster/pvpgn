#!/usr/bin/env python3
"""Differential test: EID_TALK/EID_EMOTE carry the speaker's channel flags.

When a channel OPERATOR (tmpOP, MF_GAVEL=0x02) speaks, the EID_TALK (0x05) and
EID_EMOTE (0x17) SID_CHATEVENT that other channel members receive must carry the
speaker's connection flags in the `flags` field. The original's
message_bnet_format (src/bnetd/message.cpp, message_type_talk / message_type_emote)
sets flags = conn_get_flags(me) | dstflags; the first joiner of a fresh
non-permanent channel becomes tmpOP and conn_get_flags == MF_GAVEL (0x02).

v3 previously hardcoded flags=0 on both broadcasts, so an operator's channel-mate
saw flags=0x00 instead of 0x02. This test drives an operator ("opera") talking
and emoting while a member ("memberb") reads the events, and asserts both servers
deliver flags=0x02 on the EID_TALK and EID_EMOTE memberb receives.

Only the flags field is compared: the latency field also differs (oracle
conn_get_latency=0x01 vs v3 0x00) but is non-deterministic, so it is ignored.

Run: python3 tests/diff/diff_talk_op_flags.py
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

EID_TALK = 0x05
EID_EMOTE = 0x17


def _decode(body):
    eid, flags, _lat, _ip, _acct, _reg = struct.unpack_from("<IIIIII", body, 0)
    return eid, flags


def _collect(client, settle=0.6):
    time.sleep(settle)
    old = client.sock.gettimeout()
    client.sock.settimeout(0.5)
    out = []
    try:
        while True:
            r = client.recv()
            if r is None:
                break
            sid, body = r
            if sid == bc.SID_PING:
                client.send(bc.SID_PING, body[:4])
                continue
            if sid == bc.SID_CHATEVENT:
                out.append(_decode(body))
    except Exception:
        pass
    finally:
        client.sock.settimeout(old)
    return out


def _find_flags(events, eid):
    for ev_id, flags in events:
        if ev_id == eid:
            return flags
    return None


def scenario(host, port, chan):
    # Operator "opera" joins a fresh non-permanent channel -> becomes tmpOP.
    a, _ = bc.full_login(host, port, "opera", "pass")
    bc.join_channel(a, chan)
    _collect(a, 0.3)
    # Member "memberb" joins the same channel.
    b, _ = bc.full_login(host, port, "memberb", "pass")
    bc.join_channel(b, chan)
    _collect(b, 0.3)
    _collect(a, 0.3)  # drain B-join notice on A

    # Operator talks; member reads EID_TALK.
    a.send(bc.SID_CHATCOMMAND, bc.cstring("hello channel"))
    talk_flags = _find_flags(_collect(b, 0.6), EID_TALK)

    # Operator emotes; member reads EID_EMOTE.
    a.send(bc.SID_CHATCOMMAND, bc.cstring("/me waves"))
    emote_flags = _find_flags(_collect(b, 0.6), EID_EMOTE)

    a.close()
    b.close()
    return {"talk_flags": talk_flags, "emote_flags": emote_flags}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=12240)
    ap.add_argument("--v3-port", type=int, default=12246)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port, "OpFlagChan")
        n = scenario("127.0.0.1", args.v3_port, "OpFlagChan")
        print("ORIG:", o)
        print("V3  :", n)
        # The operator's flags must be MF_GAVEL (0x02) on both servers.
        if o["talk_flags"] != 0x02 or o["emote_flags"] != 0x02:
            print("UNEXPECTED: oracle did not report MF_GAVEL — probe broken?")
            return 1
        if o == n:
            print("OK: EID_TALK/EID_EMOTE carry the operator gavel (0x02) flag.")
            return 0
        print("DIVERGENCE: v3 talk/emote flags do not match the oracle.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
