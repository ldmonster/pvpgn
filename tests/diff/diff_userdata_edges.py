#!/usr/bin/env python3
"""Differential test: READUSERDATA edge cases (nonexistent fallback + BNET\\ fields).

Beyond the happy-path write/read (diff_userdata.py), the original's
_client_statsreq has two behaviours v3 originally missed:

  C. Nonexistent-account fallback: reading a name that does not resolve to an
     account returns the CALLER's OWN profile (`if (!reqacc) reqacc = myacc;`).
  D. Auto-populated BNET\\acct\\* system fields (username/userid) are seeded at
     account creation and served on a self-read; they stay HIDDEN on a
     cross-account read.

Run: python3 tests/diff/diff_userdata_edges.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

PROFILE_KEYS = ["profile\\sex", "profile\\location", "profile\\description"]


def scenario(host, port):
    out = {}
    a, _ = bc.full_login(host, port, "alice", "pw")
    bc.write_userdata(a, "alice", {
        "profile\\sex": "Male",
        "profile\\location": "Wonderland",
        "profile\\description": "hi there",
    })
    a.close()

    b, _ = bc.full_login(host, port, "bob", "pw")
    bc.write_userdata(b, "bob", {
        "profile\\sex": "Female",
        "profile\\location": "Narnia",
        "profile\\description": "bobdesc",
    })
    out["bob_reads_alice"] = bc.read_userdata(b, "alice", PROFILE_KEYS)
    # C: nonexistent name -> caller's own profile.
    out["ghost_fallback"] = bc.read_userdata(b, "ghost_nosuchacct_12345", PROFILE_KEYS)
    # D: BNET\\ system field hidden cross-account, served on self-read.
    out["bnet_cross"] = bc.read_userdata(b, "alice", ["BNET\\acct\\username"])
    out["bnet_self"] = bc.read_userdata(b, "bob", ["BNET\\acct\\username"])

    a2, _ = bc.full_login(host, port, "alice", "pw")
    out["alice_self_userid_present"] = (
        bc.read_userdata(a2, "alice", ["BNET\\acct\\userid"])[0] != "")
    a2.close()
    b.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6382)
    ap.add_argument("--v3-port", type=int, default=6482)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        keys = ["bob_reads_alice", "ghost_fallback", "bnet_cross", "bnet_self",
                "alice_self_userid_present"]
        print(f"{'field':<26}{'match'}")
        print("-" * 70)
        all_ok = True
        for k in keys:
            same = o.get(k) == n.get(k)
            all_ok &= same
            print(f"{k:<26}{'OK' if same else 'DIFF'}")
            if not same:
                print(f"    ORACLE: {o.get(k)!r}")
                print(f"    V3    : {n.get(k)!r}")
        print()
        # Decisive: ghost falls back to bob's own profile; self BNET serves username.
        success = (all_ok
                   and o["ghost_fallback"] == o["bnet_self"][:0] + o["ghost_fallback"]
                   and o["bnet_self"] == ["bob"]
                   and o["bnet_cross"] == [""])
        if success:
            print("READUSERDATA edge cases match the oracle (fallback + BNET fields).")
            return 0
        print("FAIL: READUSERDATA edge-case divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
