#!/usr/bin/env python3
"""Differential test: CD-key proof replies (SID_CDKEY2 0x36, SID_CDKEY3 0x42).

Two pre-login CD-key proof opcodes that the original pvpgn-server ALWAYS answers
but the v3 rewrite previously left as silent no-op accepts:

    SID_CDKEY2 (0x36) CLIENT_CDKEY2 -> SERVER_CDKEYREPLY2
        The LoD second-key proof. The oracle (_client_cdkey2, handle_bnet.cpp)
        replies with message = SERVER_CDKEYREPLY2_MESSAGE_OK (0x01) followed by
        the owner string echoed back from the request.

    SID_CDKEY3 (0x42) CLIENT_CDKEY3 -> SERVER_CDKEYREPLY3
        The Diablo II 1.08+ third-key proof. The oracle (_client_cdkey3) replies
        with message = SERVER_CDKEYREPLY3_MESSAGE_OK (0x00) followed by an EMPTY
        string (the original sends "" there, not the owner).

Both handlers live in the original's "connected" (pre-login) handler table, so
the proof is sent right after the AUTH_INFO handshake, before login.

v3 status before the fix:
    - on(CdKey2Request) src/protocol/bnet/src/fsm/fsm_auth.cpp -> ok(), no reply
    - on(CdKey3Request) src/protocol/bnet/src/fsm/fsm_misc.cpp -> ok(), no reply
    A CdKey2Reply / CdKey3Reply ENCODER already existed (codec_auth.cpp), so only
    the FSM wiring was missing. A real client waits for these replies before it
    advances the login flow, so the silent accept would stall it.

Compared observables: the reply presence, the message word, and the echoed owner
string — all deterministic.

Run: python3 tests/diff/diff_cdkey.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_CDKEY2 = 0x36
SID_CDKEY3 = 0x42


def _recv_match(client, want_sid, tries=12):
    for _ in range(tries):
        r = client.recv()
        if r is None:
            return None
        sid, body = r
        if sid == bc.SID_PING:
            client.send(bc.SID_PING, body[:4])
            continue
        if sid == want_sid:
            return body
    return None


def _build_cdkey2(owner=b"alice"):
    # spawn, keylen, productid, keyvalue1, sessionkey, ticks, key_hash[5], owner
    body = struct.pack("<IIIIII", 1, 13, 0x53455850, 0, 0xDEADBEEF, 0)
    body += b"\x00" * 20
    body += owner + b"\x00"
    return body


def _build_cdkey3(owner=b""):
    body = struct.pack("<IIIIIII", 0xffffffff, 1, 0, 0x10, 6, 0x123456, 0)
    body += b"\x00" * 20
    body += owner + b"\x00"
    return body


def scenario(host, port):
    out = {}

    c = bc.BncsClient(host, port)
    bc.auth_handshake(c)
    c.send(SID_CDKEY2, _build_cdkey2(b"alice"))
    reply = _recv_match(c, SID_CDKEY2)
    if reply is not None and len(reply) >= 4:
        msg = struct.unpack_from("<I", reply, 0)[0]
        owner = reply[4:].split(b"\x00")[0].decode("latin-1", "replace")
        out["cdkey2"] = {"reply": True, "msg": msg, "owner": owner}
    else:
        out["cdkey2"] = {"reply": False}
    c.close()

    c2 = bc.BncsClient(host, port)
    bc.auth_handshake(c2)
    c2.send(SID_CDKEY3, _build_cdkey3(b""))
    reply2 = _recv_match(c2, SID_CDKEY3)
    if reply2 is not None and len(reply2) >= 4:
        msg = struct.unpack_from("<I", reply2, 0)[0]
        owner = reply2[4:].split(b"\x00")[0].decode("latin-1", "replace")
        out["cdkey3"] = {"reply": True, "msg": msg, "owner": owner}
    else:
        out["cdkey3"] = {"reply": False}
    c2.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=8740)
    ap.add_argument("--v3-port", type=int, default=8760)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<12}{'original':<40}{'v3':<40}match")
        print("-" * 100)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<12}{str(o[k]):<40}{str(n[k]):<40}{'OK' if same else 'DIFF'}")
        print()
        oracle_answers = o["cdkey2"].get("reply") and o["cdkey3"].get("reply")
        if all_ok and oracle_answers:
            print("CDKEY2 / CDKEY3 proof replies match the oracle.")
            return 0
        if not oracle_answers:
            print("FAIL: oracle did not answer — probe/setup broken.")
            return 2
        print("FAIL: v3 diverges — missing SERVER_CDKEYREPLY2 (0x36) and/or "
              "SERVER_CDKEYREPLY3 (0x42).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
