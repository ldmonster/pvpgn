#!/usr/bin/env python3
"""DIFFERENTIAL D2CS character creation: oracle (with newbie .d2s template) vs v3.

Until now the oracle could not create a character — d2char_create reads a
newbie .d2s template (newbiefile_*) that the harness did not provide, so every
CREATECHARREQ returned ALREADY_EXIST (0x14). The original server ships a real
template at files/newbie.save; the harness now points the newbiefile_* keys at
it, so the oracle creates characters for real. This unblocks the whole
char-authed surface (char select, game list, ...).

Drives CREATECHARREQ against both linked servers and asserts they agree:
  - first create (expansion bit 0x20, required by lod_realm)  -> SUCCEED 0x00
  - duplicate create                                          -> same code on both
  - char list reports >= 1 character on both
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
ALREADY_EXIST = 0x14


def oracle_flow(bnetd_port, d2cs_port):
    cli, _ = bc.full_login("127.0.0.1", bnetd_port, "ccacct", "pw", product=b"D2DV")
    rj = bc.realm_join(cli, "test", seqno=1)
    if not rj:
        cli.close(); return None
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    if c.login("ccacct", sessionnum=rj["sessionnum"], sessionkey=rj["sessionkey"],
               secret_hash_raw=rj["secret_hash"], seqno=1) != SUCCEED:
        c.close(); cli.close(); return None
    out = {
        "create": c.create_char("DiffHero", char_class=4, status=0x20),
        "dup": c.create_char("DiffHero", char_class=4, status=0x20),
    }
    lst = c.char_list()
    out["count"] = lst["currchar"] if lst else None
    c.close(); cli.close()
    return out


def v3_flow(d2cs_port):
    sn, sq = 1, 7
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    if c.login("ccacct", sessionnum=sn,
               secret_hash_raw=dc.d2cs_token("ccacct", sn, sq), seqno=sq) != SUCCEED:
        c.close(); return None
    out = {
        "create": c.create_char("DiffHero", char_class=4, status=0x20),
        "dup": c.create_char("DiffHero", char_class=4, status=0x20),
    }
    lst = c.char_list()
    out["count"] = lst["currchar"] if lst else None
    c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--bnetd-port", type=int, default=8640)
    ap.add_argument("--orig-d2cs-port", type=int, default=8660)
    ap.add_argument("--v3-d2cs-port", type=int, default=8680)
    a = ap.parse_args()

    bnetd = OriginalBnetd(a.orig_repo, a.bnetd_port,
                          realm={"name": "test", "d2cs_port": a.orig_d2cs_port})
    od = OriginalD2cs(a.orig_repo, a.orig_d2cs_port,
                      bnetd_port=a.bnetd_port, realm_name="test")
    v3 = V3D2cs(a.v3_d2cs, a.v3_d2cs_port)
    try:
        bnetd.start(); od.start(); v3.start()
        time.sleep(1.5)
        o = oracle_flow(a.bnetd_port, a.orig_d2cs_port)
        n = v3_flow(a.v3_d2cs_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")
        ok = (o is not None and n is not None and
              o["create"] == SUCCEED and n["create"] == SUCCEED and
              o["dup"] == n["dup"] and o["dup"] == ALREADY_EXIST and
              (o["count"] or 0) >= 1 and (n["count"] or 0) >= 1)
        print("OK: oracle (real .d2s template) and v3 agree on CREATECHARREQ "
              "(SUCCEED + duplicate ALREADY_EXIST)" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); od.stop(); bnetd.stop()


sys.exit(main())
