#!/usr/bin/env python3
"""After a successful OLS login, the server sends SERVER_SETEMAILREQ (0x59) when the
account has no email — BEFORE the login-ok (0x3A).

The original (loginreq2 -> client_init_email) queues an empty SERVER_SETEMAILREQ
(0x59) before the LOGINREPLY2 (0x3A) when account_get_email is empty, prompting the
client to register an address. v3 never sent it. v3 doesn't persist account email,
so a fresh account always lacks one — both servers must now send 0x59 before 0x3A.
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_SETEMAIL = 0x59


def probe(host, port):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, product=b"SEXP", client_token=ctok)
    bc.create_account_ols(c, "setm", "pw")
    h2 = bc.double_hash(bc.hash_password("pw"), ctok, stok)
    body = struct.pack("<2I", ctok, stok) + struct.pack("<5I", *h2) + bc.cstring("setm")
    c.send(bc.SID_LOGONRESPONSE2, body)
    # Record the SID sequence up to and including the login-ok (0x3A).
    seq = []
    for _ in range(8):
        r = c.recv()
        if r is None:
            break
        sid, b = r
        if sid == bc.SID_PING:
            c.send(bc.SID_PING, b[:4]); continue
        seq.append(sid)
        if sid == bc.SID_LOGONRESPONSE2:
            break
    c.close()
    has_prompt = SID_SETEMAIL in seq
    before = (has_prompt and bc.SID_LOGONRESPONSE2 in seq
              and seq.index(SID_SETEMAIL) < seq.index(bc.SID_LOGONRESPONSE2))
    return {"seq": [hex(s) for s in seq], "prompt_before_loginok": before}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6466)
    ap.add_argument("--v3-port", type=int, default=6566)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle: {o}")
        if a.orig_only:
            return 0 if o["prompt_before_loginok"] else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3    : {n}")
            ok = o["prompt_before_loginok"] and n["prompt_before_loginok"]
            print("OK: SETEMAILREQ (0x59) precedes login-ok on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
