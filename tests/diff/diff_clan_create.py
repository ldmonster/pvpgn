#!/usr/bin/env python3
"""SID_CLANCREATEREQ (0x70) always replies — clanless account / fresh tag -> CHECK_OK.

The original (clan_get_possible_member) ALWAYS answers SID_CLANCREATEREQ: when the
requested clan tag is unused AND the caller is not already in (or creating) a clan,
it replies SERVER_CLAN_CREATEREPLY with check_result=CHECK_OK (0x00) followed by the
list of eligible (mutual + online) friends — empty when the account has none. v3's
on(ClanCreateRequest) was a stub that sent nothing, so a real WC3 client opening the
create-clan dialog hung.

v3 has no clan backend (an account is always clanless and a tag is never taken), so
it always takes the CHECK_OK + empty-candidate-list path. For a fresh account with
no friends both servers must reply check_result=0x00 and zero candidate friends.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc


def probe(host, port):
    c, _ = bc.full_login(host, port, "clnc", "pw")
    bc.drain_chat(c, 0.3)
    rep = bc.clan_create_req(c, clan_tag=0x54414721, cookie=0xABCD)  # "TAG!"
    c.close()
    return rep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6456)
    ap.add_argument("--v3-port", type=int, default=6556)
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
                  and o["cookie"] == n["cookie"] == 0xABCD
                  and o["check_result"] == n["check_result"] == 0x00
                  and len(o["friends"]) == len(n["friends"]) == 0)
            print("OK: CLANCREATE replies CHECK_OK + empty candidates on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
