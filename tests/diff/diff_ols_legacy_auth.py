#!/usr/bin/env python3
"""Legacy OLS SID_LOGINREQ1 (0x29) and SID_CREATEACCTREQ2 (0x3D).

Both opcodes were no-op stubs in v3 that decoded the packet and replied with
NOTHING, hanging any client that authenticates/creates via the older OLS verbs
(instead of CREATEACCTREQ1 0x2A / LOGONRESPONSE2 0x3A). The original server
answers both. This drives BOTH servers and asserts the replies match:

  - CREATEACCTREQ2: first create -> OK (0); same name again -> EXIST (4).
  - LOGINREQ1: correct password -> SUCCESS (1); wrong password -> FAIL (0).
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_LOGINREQ1     = 0x29
SID_CREATEACCTREQ2 = 0x3D


def createacctreq2(client, username, password):
    """SID_CREATEACCTREQ2 (0x3D): hash1[5] + cstring(name). Reply result u32."""
    h1 = bc.hash_password(password)
    body = struct.pack("<5I", *h1) + bc.cstring(username)
    client.send(SID_CREATEACCTREQ2, body)
    res = bc._drain_until(client, SID_CREATEACCTREQ2)
    return bc.first_result_u32(res) if res else None


def loginreq1(client, username, password, client_token, server_token):
    """SID_LOGINREQ1 (0x29): ticks, sessionkey, hash2[5], cstring(name)."""
    h1 = bc.hash_password(password)
    h2 = bc.double_hash(h1, client_token, server_token)
    body = struct.pack("<2I", client_token, server_token)
    body += struct.pack("<5I", *h2) + bc.cstring(username)
    client.send(SID_LOGINREQ1, body)
    res = bc._drain_until(client, SID_LOGINREQ1)
    return bc.first_result_u32(res) if res else None


def probe(host, port):
    ctok = 0xDEADBEEF
    out = {}
    # --- CREATEACCTREQ2: create then duplicate ---
    c = bc.BncsClient(host, port)
    bc.auth_handshake(c, client_token=ctok)
    out["create_ok"]  = createacctreq2(c, "legacyacct", "secret")
    out["create_dup"] = createacctreq2(c, "legacyacct", "secret")
    c.close()
    # --- LOGINREQ1: good then bad password (fresh connection + handshake) ---
    c = bc.BncsClient(host, port)
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    out["login_ok"]  = loginreq1(c, "legacyacct", "secret", ctok, stok)
    c.close()
    c = bc.BncsClient(host, port)
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    out["login_bad"] = loginreq1(c, "legacyacct", "wrongpw", ctok, stok)
    c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6413)
    ap.add_argument("--v3-port", type=int, default=6513)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = probe("127.0.0.1", a.orig_port)
        n = probe("127.0.0.1", a.v3_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")
        # v3 must now REPLY to both opcodes (was None before the fix).
        replies_present = all(v is not None for v in n.values())
        # Structural parity: create OK==OK, dup==dup, good login==good, bad==bad.
        parity = (
            n["create_ok"] == o["create_ok"] and
            n["create_dup"] == o["create_dup"] and
            n["login_ok"] == o["login_ok"] and
            n["login_bad"] == o["login_bad"]
        )
        ok = replies_present and parity
        print("OK: v3 answers 0x29/0x3D and matches the oracle" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
