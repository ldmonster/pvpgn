#!/usr/bin/env python3
"""SID_FRIENDSLIST location: a friend in a channel reports CHAT (0x02) + name.

The original sets a friend's location to FRIENDSTATUS_CHAT (0x02) with the channel
name as the location-name string. v3 previously reported CHAT but an empty name
(and only when current_channel happened to be set, which it never was). Now
ListFriends resolves the friend's channel from the shared channel repo.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

LOC_CHAT = 0x02


def probe(host, port):
    ca, _ = bc.full_login(host, port, "aloc", "pw")
    cb, _ = bc.full_login(host, port, "bloc", "pw")
    bc.friends_add(ca, "bloc")          # A lists B
    bc.join_channel(cb, "BLocChan")     # B is now in a channel
    lst = bc.request_friends_list(ca)
    ca.close(); cb.close()
    b = next((f for f in lst if f["name"].lower() == "bloc"), None)
    return b  # {name, status, location}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6436)
    ap.add_argument("--v3-port", type=int, default=6536)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = probe("127.0.0.1", a.orig_port)
        n = probe("127.0.0.1", a.v3_port)
        print(f"oracle B: {o}")
        print(f"v3     B: {n}")
        ok = (o and n and o["location"] == LOC_CHAT and n["location"] == LOC_CHAT)
        print("OK: friend-in-channel reports CHAT (0x02) on both" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
