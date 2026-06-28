#!/usr/bin/env python3
"""SID_CLANMEMBER_RANKUPDATE_REQ (0x7A) always replies — clanless caller -> FAILED.

The original (_client_clanmember_rankupdatereq) ALWAYS answers with
SERVER_CLANMEMBER_RANKUPDATE_REPLY (count + result). A clanless caller (no
account_get_clan) falls through to result=RANKUPDATE_FAILED (0x01). v3's
on(ClanMemberRankUpdateRequest) was a stub that sent nothing -> client hang.

v3 has no clan backend so every caller is clanless: both servers must reply with
the cookie echoed and result=0x01 (FAILED).
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc


def probe(host, port):
    c, _ = bc.full_login(host, port, "clrk", "pw")
    bc.drain_chat(c, 0.3)
    rep = bc.clan_rankupdate_req(c, name="nobody", new_rank=2, cookie=0x99)
    c.close()
    return rep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6458)
    ap.add_argument("--v3-port", type=int, default=6558)
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
                  and o["cookie"] == n["cookie"] == 0x99
                  and o["result"] == n["result"] == 0x01)
            print("OK: RANKUPDATE replies cookie + FAILED(0x01) on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
