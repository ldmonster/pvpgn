#!/usr/bin/env python3
"""SID_FRIENDSLIST per-friend status byte: FRIEND_TYPE_MUTUAL (0x01).

The original sets bit 0x01 in a friend's status when that friend ALSO lists the
owner. v3 previously hardcoded status=0. With A<->B mutual and A->C one-way, A's
friends list must show B's status with the mutual bit set and C's without it.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

MUTUAL = 0x01


def probe(host, port):
    ca, _ = bc.full_login(host, port, "amut", "pw")
    cb, _ = bc.full_login(host, port, "bmut", "pw")
    cc, _ = bc.full_login(host, port, "cmut", "pw")
    bc.friends_add(ca, "bmut")   # A lists B
    bc.friends_add(ca, "cmut")   # A lists C
    bc.friends_add(cb, "amut")   # B lists A  -> A<->B mutual; C never lists A
    lst = bc.request_friends_list(ca)
    ca.close(); cb.close(); cc.close()
    by = {f["name"].lower(): f for f in lst}
    return {
        "b_mutual": bool(by.get("bmut", {}).get("status", 0) & MUTUAL),
        "c_mutual": bool(by.get("cmut", {}).get("status", 0) & MUTUAL),
        "count": len(lst),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6434)
    ap.add_argument("--v3-port", type=int, default=6534)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = probe("127.0.0.1", a.orig_port)
        n = probe("127.0.0.1", a.v3_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")
        ok = (o["b_mutual"] and not o["c_mutual"] and
              n["b_mutual"] and not n["c_mutual"])
        print("OK: mutual status bit matches oracle (B mutual, C not)"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
