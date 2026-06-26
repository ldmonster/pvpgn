#!/usr/bin/env python3
"""Differential test: file-info / icon metadata replies.

Two early-handshake metadata opcodes that the original pvpgn-server answers but
the v3 rewrite currently leaves as no-op stubs:

    SID_GETFILETIME (0x33) CLIENT_FILEINFOREQ -> SERVER_FILEINFOREPLY
        The client asks for the mod-time of a named file (TOS, gateways, icons).
        The oracle ALWAYS replies, echoing the request's `type` and `unknown2`
        words, a file timestamp, and the filename string. Registered in both the
        connected and logged-in handler tables, so it works pre- and post-login.

    SID_GETICONDATA (0x2D) CLIENT_ICONREQ -> SERVER_ICONREPLY
        The client (connected/pre-login) asks for the icons.bni metadata; the
        oracle replies with a timestamp + the configured icon filename.
        Registered ONLY in the connected table, so it must be sent before login.

v3 status (see root-cause):
    - on(FileInfoRequest)  src/protocol/bnet/src/fsm/fsm_auth.cpp:236 -> ok(), no reply
    - on(IconRequest)      src/protocol/bnet/src/fsm/fsm_misc.cpp:77  -> ok(), no reply
    A FileInfoReply / IconReply ENCODER already exists in the codec
    (codec_auth.cpp decode_file_info_reply, codec_ladder.cpp encode(IconRequest)),
    so only the FSM wiring is missing.

This is a RED test: it documents the gap. It goes GREEN once v3's handlers send
the replies the oracle does. The compared observables are deterministic (the
echoed type/unknown2/filename), NOT the timestamp (a host FILETIME that legitimately
differs between servers / file systems).

Run: python3 tests/diff/diff_fileinfo.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_FILEINFO = 0x33
SID_ICONREQ = 0x2D


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


def scenario(host, port):
    out = {}

    # --- SID_GETFILETIME (0x33), post-login ---
    c, _ = bc.full_login(host, port, "alice", "alicepass")
    req = struct.pack("<II", 0x1A, 0) + bc.cstring("tos_USA.txt")  # type=TOS
    c.send(SID_FILEINFO, req)
    reply = _recv_match(c, SID_FILEINFO)
    if reply is not None and len(reply) >= 16:
        typ, unk, _ts = struct.unpack_from("<IIQ", reply, 0)
        fn = reply[16:].split(b"\x00")[0].decode("latin-1", "replace")
        out["fileinfo"] = {"reply": True, "type": typ, "unk": unk, "fn": fn}
    else:
        out["fileinfo"] = {"reply": False}
    c.close()

    # --- SID_GETICONDATA (0x2D), pre-login (connected state) ---
    c2 = bc.BncsClient(host, port)
    bc.auth_handshake(c2, client_token=0xDEADBEEF)
    c2.send(SID_ICONREQ, b"")  # empty body
    reply2 = _recv_match(c2, SID_ICONREQ)
    if reply2 is not None and len(reply2) >= 8:
        fn = reply2[8:].split(b"\x00")[0].decode("latin-1", "replace")
        out["iconreq"] = {"reply": True, "fn": fn}
    else:
        out["iconreq"] = {"reply": False}
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
    ap.add_argument("--orig-port", type=int, default=8640)
    ap.add_argument("--v3-port", type=int, default=8660)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<24}{'original':<40}{'v3':<40}match")
        print("-" * 110)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<24}{str(o[k]):<40}{str(n[k]):<40}{'OK' if same else 'DIFF'}")
        print()
        # Sanity: the oracle must actually answer both (guards the probe itself).
        oracle_answers = o["fileinfo"].get("reply") and o["iconreq"].get("reply")
        if all_ok and oracle_answers:
            print("File-info / icon metadata replies match the oracle.")
            return 0
        if not oracle_answers:
            print("FAIL: oracle did not answer — probe/setup broken.")
            return 2
        print("FAIL: v3 diverges — missing SERVER_FILEINFOREPLY (0x33) and/or "
              "SERVER_ICONREPLY (0x2D). v3's on(FileInfoRequest)/on(IconRequest) "
              "are no-op stubs (fsm_auth.cpp:236, fsm_misc.cpp:77).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
