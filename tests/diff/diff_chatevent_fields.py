#!/usr/bin/env python3
"""Differential test: SID_CHATEVENT (0x0F) fixed wire-field fidelity.

Drives, against BOTH the original pvpgn-server and v3:

    alice logs in, joins a channel, sends "/time"  (success -> EID_INFO 0x12)
    then sends "/zzznotacommand" (unknown    -> EID_ERROR 0x13)

The original's message_bnet_format (bnetd/message.cpp) writes a FIXED wire
layout into EVERY SID_CHATEVENT, independent of the localized text:

  * player_ip   = SERVER_MESSAGE_PLAYER_IP_DUMMY (0)
  * account_num = SERVER_MESSAGE_ACCOUNT_NUM (bn_int_nset 0x0df0adba)
  * reg_auth    = SERVER_MESSAGE_REG_AUTH   (bn_int_set  0xBAADF00D)

Both account_num and reg_auth deliberately emit the identical 4 on-wire bytes
0D F0 AD BA, which a little-endian reader decodes as 0xBAADF00D. For
message_type_info AND message_type_error the username is an EMPTY string ("").

This guards the previously-diverging v3 fields (acct/reg formerly 0xBADC0FFE,
and the EID_INFO username formerly the literal "Battle.net"). We compare the
fixed header fields (flags/latency/player_ip/account_num/reg_auth) and the
username BYTE-FOR-BYTE; the localized text is charset-garbled under this mock
harness, so only its presence/non-emptiness is checked, not its bytes.

Run: python3 tests/diff/diff_chatevent_fields.py
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

CHAN = "FieldChan"
EID_INFO = 0x12
EID_ERROR = 0x13
WIRE_ACCT = 0xBAADF00D   # LE decode of on-wire 0D F0 AD BA
WIRE_REG = 0xBAADF00D


def collect_chatevents(client, settle=0.5):
    """Collect raw SID_CHATEVENT bodies, answering PINGs."""
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
                out.append(body)
    finally:
        client.sock.settimeout(old)
    return out


def decode(body):
    eid, flags, lat, ip, acct, reg = struct.unpack_from("<IIIIII", body, 0)
    parts = body[24:].split(b"\x00")
    uname = parts[0] if len(parts) > 0 else b""
    text = parts[1] if len(parts) > 1 else b""
    return dict(eid=eid, flags=flags, lat=lat, ip=ip, acct=acct, reg=reg,
                uname=uname, text=text)


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.join_channel(alice, CHAN)
    collect_chatevents(alice, settle=0.3)  # drain join roster noise
    alice.send(bc.SID_CHATCOMMAND, bc.cstring("/time"))
    info = collect_chatevents(alice)
    alice.send(bc.SID_CHATCOMMAND, bc.cstring("/zzznotacommand"))
    err = collect_chatevents(alice)
    alice.close()
    return info, err


def first_with_eid(events, eid):
    for b in events:
        d = decode(b)
        if d["eid"] == eid:
            return d
    return None


def check(label, d):
    """Verify a single decoded event against the fixed oracle wire layout."""
    problems = []
    if d is None:
        return [f"{label}: no matching event received"]
    if d["flags"] != 0:
        problems.append(f"{label}: flags=0x{d['flags']:08x} (want 0)")
    if d["lat"] != 0:
        problems.append(f"{label}: latency=0x{d['lat']:08x} (want 0)")
    if d["ip"] != 0:
        problems.append(f"{label}: player_ip=0x{d['ip']:08x} (want 0)")
    if d["acct"] != WIRE_ACCT:
        problems.append(
            f"{label}: account_num=0x{d['acct']:08x} (want 0x{WIRE_ACCT:08x})")
    if d["reg"] != WIRE_REG:
        problems.append(
            f"{label}: reg_auth=0x{d['reg']:08x} (want 0x{WIRE_REG:08x})")
    if d["uname"] != b"":
        problems.append(f"{label}: username={d['uname']!r} (want b'')")
    if not d["text"]:
        problems.append(f"{label}: empty text (expected a message body)")
    return problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=10880)
    ap.add_argument("--v3-port", type=int, default=10892)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o_info, o_err = scenario("127.0.0.1", args.orig_port)
        n_info, n_err = scenario("127.0.0.1", args.v3_port)

        oi = first_with_eid(o_info, EID_INFO)
        ni = first_with_eid(n_info, EID_INFO)
        oe = first_with_eid(o_err, EID_ERROR)
        ne = first_with_eid(n_err, EID_ERROR)

        def show(tag, d):
            if d is None:
                print(f"  {tag}: <none>")
                return
            print(f"  {tag}: eid=0x{d['eid']:02x} flags=0x{d['flags']:08x} "
                  f"lat=0x{d['lat']:08x} ip=0x{d['ip']:08x} "
                  f"acct=0x{d['acct']:08x} reg=0x{d['reg']:08x} "
                  f"uname={d['uname']!r}")

        print("EID_INFO (/time):")
        show("oracle", oi)
        show("v3    ", ni)
        print("EID_ERROR (/zzznotacommand):")
        show("oracle", oe)
        show("v3    ", ne)

        problems = []
        # The oracle must itself exhibit the documented layout (sanity anchor).
        problems += check("oracle EID_INFO", oi)
        problems += check("oracle EID_ERROR", oe)
        # v3 must match it field-for-field.
        problems += check("v3 EID_INFO", ni)
        problems += check("v3 EID_ERROR", ne)

        # Cross-check: the fixed header bytes must be byte-for-byte identical.
        for tag, od, nd in (("EID_INFO", oi, ni), ("EID_ERROR", oe, ne)):
            if od is None or nd is None:
                continue
            for fld in ("flags", "lat", "ip", "acct", "reg", "uname"):
                if od[fld] != nd[fld]:
                    problems.append(
                        f"{tag}: field {fld} oracle={od[fld]!r} v3={nd[fld]!r}")

        if problems:
            print("FAIL:")
            for p in problems:
                print("  - " + p)
            return 1
        print("OK: SID_CHATEVENT fixed fields (acct/reg/ip/flags/uname) match "
              "the oracle for EID_INFO and EID_ERROR.")
        return 0
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
