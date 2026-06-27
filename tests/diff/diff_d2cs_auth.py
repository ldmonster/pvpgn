#!/usr/bin/env python3
"""DIFFERENTIAL D2CS login authentication: oracle (bnetd<->d2cs linked) vs v3.

The ORIGINAL d2cs authenticates a client LOGINREQ against bnetd: the client does
a BNCS login, then SID_LOGONREALMEX (realm-join) where bnetd ISSUES a secret_hash
(salt=join-seqno hashed with the session secret + password), then presents that
secret_hash to d2cs, which forwards it to bnetd for re-derivation + comparison.

This drives that full real path against the linked original servers and asserts
the auth actually discriminates:
  - valid credentials           -> LOGINREPLY 0x00 SUCCEED
  - tampered secret_hash        -> LOGINREPLY 0x0c BADPASS
  - wrong account name          -> LOGINREPLY 0x0c BADPASS

v3's d2cs auth is currently a permissive STUB (always success), so it accepts a
valid login (parity for the happy path) but does NOT yet reject tampered/forged
credentials. That gap is reported here and is the target of the "real v3 d2cs
auth" work; once implemented, the v3 tampered/wrong-account cases should also
return 0x0c and this test can assert full parity.

NOTE: CREATECHARREQ is NOT exercised here — the original d2cs needs real D2
newbie .d2s save-file templates to create a character (file_read(newbiefile)
failure makes d2char_create return ALREADY_EXIST 0x14), a binary asset the
harness cannot synthesise. Auth + char-list are the asset-free differential.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from d2cs_server import OriginalD2cs, V3D2cs  # noqa: E402
import bncs_client as bc  # noqa: E402
import d2cs_client as dc  # noqa: E402

SUCCEED = 0x00
BADPASS = 0x0c


def oracle_auth(bnetd_port, d2cs_port):
    cli, _ = bc.full_login("127.0.0.1", bnetd_port, "d2auth", "pw", product=b"D2DV")
    rj = bc.realm_join(cli, "test", seqno=1)
    if not rj:
        cli.close()
        return None
    out = {}
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    out["valid"] = c.login("d2auth", sessionnum=rj["sessionnum"],
                           sessionkey=rj["sessionkey"],
                           secret_hash_raw=rj["secret_hash"], seqno=1)
    c.close()
    bad = bytes((rj["secret_hash"][0] ^ 0xFF,)) + rj["secret_hash"][1:]
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    out["tampered"] = c.login("d2auth", sessionnum=rj["sessionnum"],
                              sessionkey=rj["sessionkey"],
                              secret_hash_raw=bad, seqno=1)
    c.close()
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    out["wrong_account"] = c.login("nobody", sessionnum=rj["sessionnum"],
                                   sessionkey=rj["sessionkey"],
                                   secret_hash_raw=rj["secret_hash"], seqno=1)
    c.close()
    cli.close()
    return out


def v3_auth(d2cs_port):
    out = {}
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    out["valid"] = c.login("d2auth")
    c.close()
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    out["tampered"] = c.login("d2auth", secret_hash_raw=b"\xde\xad" + b"\x00" * 18)
    c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--bnetd-port", type=int, default=8300)
    ap.add_argument("--orig-d2cs-port", type=int, default=8320)
    ap.add_argument("--v3-d2cs-port", type=int, default=8340)
    a = ap.parse_args()

    bnetd = OriginalBnetd(a.orig_repo, a.bnetd_port,
                          realm={"name": "test", "d2cs_port": a.orig_d2cs_port})
    od2cs = OriginalD2cs(a.orig_repo, a.orig_d2cs_port,
                         bnetd_port=a.bnetd_port, realm_name="test")
    v3 = V3D2cs(a.v3_d2cs, a.v3_d2cs_port)
    try:
        bnetd.start()
        od2cs.start()
        v3.start()
        time.sleep(1.5)  # let the d2cs<->bnetd link authenticate

        o = oracle_auth(a.bnetd_port, a.orig_d2cs_port)
        n = v3_auth(a.v3_d2cs_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")

        # Required (passes today): the oracle does REAL auth and v3 accepts a
        # valid login (happy-path parity).
        oracle_real = (o and o["valid"] == SUCCEED and
                       o["tampered"] == BADPASS and o["wrong_account"] == BADPASS)
        v3_accepts_valid = n["valid"] == SUCCEED
        ok = oracle_real and v3_accepts_valid

        v3_rejects_tampered = n["tampered"] == BADPASS
        print(f"oracle real-auth (valid=0/tampered=0x0c/wrong=0x0c): {oracle_real}")
        print(f"v3 accepts valid: {v3_accepts_valid}; "
              f"v3 rejects tampered: {v3_rejects_tampered} "
              f"({'real auth' if v3_rejects_tampered else 'STUB — Stage 2 target'})")
        print("OK: oracle bnetd<->d2cs auth verified; v3 happy-path parity"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop()
        od2cs.stop()
        bnetd.stop()


sys.exit(main())
