#!/usr/bin/env python3
"""SID_LOGONRESPONSE2 (0x3A) for an invalid-character username: result only, no reason.

The original's SERVER_LOGINREPLY2 is a fixed-size packet carrying just the 4-byte
message code — it never appends a reason string. An invalid-character username
(e.g. "/") can't name an existing account, so accountlist_find_account returns NULL
and the oracle answers NONEXIST (0x01) with NO trailing bytes. v3 used to append a
"Invalid username" reason string (within the 0x3A packet) on that path. Both must
now send result=0x01 and an identical (empty) trailing payload.
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc


def probe(host, port):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, product=b"SEXP", client_token=ctok)
    # LOGONRESPONSE2 body with an invalid-character username "/".
    h2 = bc.double_hash(bc.hash_password("pw"), ctok, stok)
    body = struct.pack("<2I", ctok, stok) + struct.pack("<5I", *h2) + bc.cstring("/")
    c.send(bc.SID_LOGONRESPONSE2, body)
    reply = bc._drain_until(c, bc.SID_LOGONRESPONSE2)
    c.close()
    if reply is None or len(reply) < 4:
        return None
    result = struct.unpack_from("<I", reply, 0)[0]
    trailing = reply[4:]          # any bytes after the result code
    return {"result": result, "trailing_len": len(trailing)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6462)
    ap.add_argument("--v3-port", type=int, default=6562)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle: {o}")
        if a.orig_only:
            return 0 if o else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3    : {n}")
            ok = (o is not None and n is not None
                  and o["result"] == n["result"]
                  and o["trailing_len"] == n["trailing_len"] == 0)
            print("OK: invalid-username LOGONRESPONSE2 is result-only on both (no reason)"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
