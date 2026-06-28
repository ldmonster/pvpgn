#!/usr/bin/env python3
"""SID_LOGONREALMEX (0x3E) always replies, even when the realm doesn't exist.

The original (_client_realmjoinreq109) ALWAYS answers a realm-join: when the
requested realm is not found it still sends SERVER_REALMJOINREPLY_109 with the
seqno echoed and every other field zeroed (+ empty account name). v3 previously
left on(RealmJoinRequest) as a no-op (no reply at all) -> a real D2 client hangs
after BNCS login waiting for the realm-join result.

Neither harness server has an active realm configured, so both must take the
"no active realm" path: a zeroed reply (sessionnum/sessionkey/addr/port all 0).
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc


def probe(host, port):
    c, _ = bc.full_login(host, port, "rjn", "pw")
    rep = bc.realm_join(c, "NoSuchRealm", seqno=7)
    c.close()
    return rep


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6450)
    ap.add_argument("--v3-port", type=int, default=6550)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle: {o}")
        if a.orig_only:
            return 0
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3    : {n}")
            # Both must REPLY (not None) with the no-realm zeroed result.
            ok = (o is not None and n is not None
                  and o["sessionnum"] == 0 and n["sessionnum"] == 0
                  and o["sessionkey"] == 0 and n["sessionkey"] == 0
                  and o["d2cs_addr"] == n["d2cs_addr"]
                  and o["d2cs_port"] == n["d2cs_port"] == 0)
            print("OK: realm-join no-realm reply matches the oracle (both reply, zeroed)"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
