#!/usr/bin/env python3
"""SID_FRIENDSLIST per-friend status bits + clienttag from the friend's live conn.

Old server (handle_bnet.cpp _client_friendslistreq): for each friend it reads the
friend's CONNECTION — conn_get_clienttag (the product tag), conn_get_dndstr (=>
FRIEND_TYPE_DND 0x02), conn_get_awaystr (=> FRIEND_TYPE_AWAY 0x04) — and ORs the
away/dnd bits into the status byte (alongside FRIEND_TYPE_MUTUAL 0x01). v3 used to
hardcode the away/dnd bits to 0 and the clienttag to 0.

v3 now publishes each logged-in session's clienttag/away/dnd into a presence store
that ListFriends reads back, so a friend who is /away or /dnd shows the right bit
and a real product tag.

Scenario: A lists B (one-directional, so NOT mutual — isolates the away/dnd bits).
B sets /dnd then /away; A re-reads the friends list. Both servers must report the
same status bits and the same (nonzero) clienttag for B.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

FRIEND_TYPE_DND  = 0x02
FRIEND_TYPE_AWAY = 0x04


def probe(host, port):
    ca, _ = bc.full_login(host, port, "afa", "pw")
    cb, _ = bc.full_login(host, port, "bfa", "pw")
    bc.friends_add(ca, "bfa")               # A lists B (one-way)

    # Baseline: B online, available.
    base = bc.request_friends_list(ca)
    b0 = next((f for f in base if f["name"].lower() == "bfa"), None)

    # B marks itself DND then AWAY.
    bc.chat_command(cb, "/dnd busy")
    bc.chat_command(cb, "/away afk")
    after = bc.request_friends_list(ca)
    b1 = next((f for f in after if f["name"].lower() == "bfa"), None)

    ca.close(); cb.close()
    return b0, b1


def summarize(b0, b1):
    if not b0 or not b1:
        return None
    return {
        "base_dnd":   bool(b0["status"] & FRIEND_TYPE_DND),
        "base_away":  bool(b0["status"] & FRIEND_TYPE_AWAY),
        "base_tag":   b0["client_tag"],
        "dnd_bit":    bool(b1["status"] & FRIEND_TYPE_DND),
        "away_bit":   bool(b1["status"] & FRIEND_TYPE_AWAY),
        "after_tag":  b1["client_tag"],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6440)
    ap.add_argument("--v3-port", type=int, default=6540)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = summarize(*probe("127.0.0.1", a.orig_port))
        print(f"oracle B: {o}")
        if a.orig_only:
            return 0
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = summarize(*probe("127.0.0.1", a.v3_port))
            print(f"v3     B: {n}")
            ok = (o and n
                  # baseline: available, real product tag
                  and o["base_dnd"] is False and n["base_dnd"] is False
                  and o["base_away"] is False and n["base_away"] is False
                  and o["base_tag"] != 0 and n["base_tag"] == o["base_tag"]
                  # after /dnd + /away: both bits set on both servers
                  and o["dnd_bit"] and n["dnd_bit"]
                  and o["away_bit"] and n["away_bit"]
                  and o["after_tag"] == n["after_tag"])
            print("OK: away/dnd bits + clienttag match the oracle"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
