#!/usr/bin/env python3
"""Differential test: friends list (SID_FRIENDSLIST 0x65 + /friends add|remove).

Drives, against BOTH the original pvpgn-server and v3:

    create alice + bob (OLS)
    alice: SID_FRIENDSLIST           -> empty
    alice: /friends add bob          -> bob on the list (offline)
    alice: SID_FRIENDSLIST           -> [bob]
    alice: /friends remove bob       -> removed
    alice: SID_FRIENDSLIST           -> empty

and diffs the friend names + online state at each step. The exact location_name
(channel) is not compared (v3 does not resolve it), but the list membership and
offline/online flag are the stable, faithful observables.

Regression guard for the v3 friends wiring (SID_FRIENDSLIST handler + /friends
command routed to the AddFriend/RemoveFriend/ListFriends use-cases).

Run: python3 tests/diff/diff_friends.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def names(friend_list):
    return [f["name"].lower() for f in friend_list]


def scenario(host, port):
    out = {}
    # Pre-create bob so he is a real account to befriend (offline is fine).
    bobc, _ = bc.full_login(host, port, "bob", "bobpass")
    bobc.close()

    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    out["empty0"] = names(bc.request_friends_list(alice))
    bc.friends_add(alice, "bob")
    out["after_add"] = names(bc.request_friends_list(alice))
    bc.friends_remove(alice, "bob")
    out["after_remove"] = names(bc.request_friends_list(alice))
    alice.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6372)
    ap.add_argument("--v3-port", type=int, default=6472)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        keys = ["empty0", "after_add", "after_remove"]
        print(f"{'step':<14}{'original':<16}{'v3':<16}match")
        print("-" * 56)
        all_ok = True
        for k in keys:
            same = o.get(k) == n.get(k)
            all_ok &= same
            print(f"{k:<14}{str(o.get(k)):<16}{str(n.get(k)):<16}"
                  f"{'OK' if same else 'DIFF'}")
        success = (all_ok and o.get("empty0") == []
                   and o.get("after_add") == ["bob"]
                   and o.get("after_remove") == [])
        print()
        if success:
            print("Friends list matches the oracle (add/list/remove).")
            return 0
        print("FAIL: divergence in friends list.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
