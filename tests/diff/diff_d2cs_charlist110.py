#!/usr/bin/env python3
"""DIFFERENTIAL D2CS CHARLISTREQ_110 (0x19): oracle (bnetd<->d2cs linked) vs v3.

A 1.10+ client sends CHARLISTREQ_110 (0x19) after logging in. The reply MUST be
a CHARLISTREPLY_110 (packet type 0x19), NOT the 0x17 reply, and each per-char
entry carries a leading 4-byte expire_time. With an empty character list (no
.d2s assets required), both servers emit the 0x19 base packet with currchar=0.

This locks the wave-212 fix under e2e coverage: before it, v3 answered a 0x19
request with a (doubled) 0x17 reply, so a 1.10+ client misparsed the response.
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


def oracle_charlist110(bnetd_port, d2cs_port):
    cli, _ = bc.full_login("127.0.0.1", bnetd_port, "d2list", "pw", product=b"D2DV")
    rj = bc.realm_join(cli, "test", seqno=1)
    if not rj:
        cli.close()
        return None
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    login = c.login("d2list", sessionnum=rj["sessionnum"],
                    sessionkey=rj["sessionkey"],
                    secret_hash_raw=rj["secret_hash"], seqno=1)
    reply = c.char_list_110() if login == SUCCEED else None
    c.close()
    cli.close()
    return {"login": login, "reply": reply}


def v3_charlist110(d2cs_port):
    sn, sq = 1, 7
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    login = c.login("d2list", sessionnum=sn,
                    secret_hash_raw=dc.d2cs_token("d2list", sn, sq), seqno=sq)
    reply = c.char_list_110() if login == SUCCEED else None
    c.close()
    return {"login": login, "reply": reply}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--bnetd-port", type=int, default=8360)
    ap.add_argument("--orig-d2cs-port", type=int, default=8380)
    ap.add_argument("--v3-d2cs-port", type=int, default=8400)
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
        time.sleep(1.5)

        o = oracle_charlist110(a.bnetd_port, a.orig_d2cs_port)
        n = v3_charlist110(a.v3_d2cs_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")

        # recv_type(0x19) only returns when the reply's TYPE byte is 0x19, so a
        # non-None reply on each side already proves the 0x19 type. Assert both
        # logged in, both produced a 0x19 reply, and both report an empty list.
        def ok_side(r):
            return (r is not None and r["login"] == SUCCEED and
                    r["reply"] is not None and r["reply"]["currchar"] == 0)

        ok = ok_side(o) and ok_side(n)
        print(f"oracle 0x19 empty-list reply: {ok_side(o)}")
        print(f"v3     0x19 empty-list reply: {ok_side(n)}")
        print("OK: both d2cs servers answer CHARLISTREQ_110 with a 0x19 reply"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop()
        od2cs.stop()
        bnetd.stop()


sys.exit(main())
