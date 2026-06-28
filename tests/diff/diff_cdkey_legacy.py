#!/usr/bin/env python3
"""SID_CDKEY (0x30) legacy CD-key check: server always replies CDKEYREPLY.

The original _client_cdkey ALWAYS sends SERVER_CDKEYREPLY with MESSAGE_OK (0x01)
and the owner string echoed back (it stores the owner and does not validate the
key under the test config). v3 had a no-op stub that sent nothing, hanging the
client. Both servers must reply (OK, owner).
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_CDKEY = 0x30
CDKEY_OK = 0x01


def cdkey_reply(host, port, owner):
    c = bc.BncsClient(host, port)
    # spawn(4) + cdkey cstr + owner cstr
    body = struct.pack("<I", 0) + bc.cstring("1234567890123") + bc.cstring(owner)
    c.send(SID_CDKEY, body)
    rep = bc._drain_until(c, SID_CDKEY)
    c.close()
    if rep is None or len(rep) < 4:
        return None
    msg = struct.unpack_from("<I", rep, 0)[0]
    echoed = rep[4:].split(b"\x00")[0].decode("latin-1", "replace")
    return (msg, echoed)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6422)
    ap.add_argument("--v3-port", type=int, default=6522)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = cdkey_reply("127.0.0.1", a.orig_port, "OwnerGuy")
        n = cdkey_reply("127.0.0.1", a.v3_port, "OwnerGuy")
        print(f"CDKEYREPLY (message, owner): oracle={o!r} v3={n!r}")
        ok = (o == (CDKEY_OK, "OwnerGuy") and n == (CDKEY_OK, "OwnerGuy"))
        print("OK: both reply CDKEYREPLY OK + owner echoed" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
