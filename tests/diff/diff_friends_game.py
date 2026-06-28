#!/usr/bin/env python3
"""SID_FRIENDSLIST location: a friend hosting a game reports PUBLIC_GAME (0x03)
+ the game name as the location string.

Old server (handle_bnet.cpp _client_friendslistreq): if conn_get_game(dest) is
non-null, location = FRIENDSTATUS_PUBLIC_GAME (0x03) unless game_flag_private,
then PRIVATE_GAME (0x05). Game takes precedence over channel. STARTADVEX3 calls
conn_set_game, so advertising a (passwordless) game sets a public game location.

v3 ListFriends previously only resolved channel location; this adds the game
slice (scan IGameRepository.list_active() for a game the friend is in).
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

LOC_PUBLIC_GAME = 0x03


def probe(host, port):
    ca, _ = bc.full_login(host, port, "agm", "pw")
    cb, _ = bc.full_login(host, port, "bgm", "pw")
    bc.friends_add(ca, "bgm")               # A lists B
    bc.advertise_game(cb, "BGmGame")        # B now hosts a public game
    lst = bc.request_friends_list(ca)
    ca.close(); cb.close()
    b = next((f for f in lst if f["name"].lower() == "bgm"), None)
    return b  # {name, status, location}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6438)
    ap.add_argument("--v3-port", type=int, default=6538)
    ap.add_argument("--orig-only", action="store_true",
                    help="probe the old server only (capture expected behavior)")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle B: {o}")
        if a.orig_only:
            ok = bool(o) and o["location"] == LOC_PUBLIC_GAME
            print("OK: oracle reports PUBLIC_GAME (0x03)" if ok else "FAIL: oracle")
            return 0 if ok else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3     B: {n}")
            ok = (o and n and o["location"] == LOC_PUBLIC_GAME
                  and n["location"] == LOC_PUBLIC_GAME)
            print("OK: friend-in-game reports PUBLIC_GAME (0x03) on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
