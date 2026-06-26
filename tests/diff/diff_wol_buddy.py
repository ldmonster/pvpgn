#!/usr/bin/env python3
"""Differential test: WOL buddy list (ADDBUDDY / DELBUDDY / GETBUDDY).

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client B: WOL login                 (so account "wolbuddyb" exists)
    client A: WOL login
    client A: ADDBUDDY wolbuddyb   -> 334
    client A: GETBUDDY            -> list contains wolbuddyb
    client A: DELBUDDY wolbuddyb   -> 335
    client A: GETBUDDY            -> list no longer contains wolbuddyb

and diffs whether the buddy appears after ADD and disappears after DEL. A match
means v3's WOL buddy commands drive the same friend-list store the way the
original does (they reuse v3's AddFriend/RemoveFriend/ListFriends use-cases).

Run: python3 tests/diff/diff_wol_buddy.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

BUDDY = "wolbuddyb"


def scenario(host, port, sku=1000):
    """Returns {add_code, has_after_add, del_code, has_after_del}."""
    b = wc.wol_session(host, port, BUDDY, "secretpass", sku=sku)
    a = wc.wol_session(host, port, "wolbuddya", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {"add_code": None, "has_after_add": None,
                "del_code": None, "has_after_del": None}
    try:
        add_code = wc.wol_addbuddy(a, BUDDY)
        after_add = wc.wol_getbuddy(a)
        del_code = wc.wol_delbuddy(a, BUDDY)
        after_del = wc.wol_getbuddy(a)
        return {
            "add_code": add_code,
            "has_after_add": (BUDDY in after_add) if after_add is not None else None,
            "del_code": del_code,
            "has_after_del": (BUDDY in after_del) if after_del is not None else None,
        }
    finally:
        a.close()
        b.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6396)
    ap.add_argument("--v3-port", type=int, default=6496)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<16}{'original':<14}{'v3':<14}match")
        print("-" * 54)
        ok = True
        for k in ("add_code", "has_after_add", "del_code", "has_after_del"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<16}{ov:<14}{nv:<14}{'OK' if m else 'DIFF'}")
        print()

        if (ok and o["has_after_add"] is True and o["has_after_del"] is False):
            print("WOL buddy list matches the oracle (add shows, del removes).")
            return 0
        print("FAIL: WOL buddy list divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
