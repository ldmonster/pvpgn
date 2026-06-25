#!/usr/bin/env python3
"""Differential test: distinct channels stay isolated.

Alice joins "RED", Bob joins "BLUE", then Carol joins "RED". Carol's channel
roster (EID_SHOWUSER set) must contain exactly {alice, carol} on both servers --
Bob (in BLUE) must NOT leak into RED.

Regression guard for bug-hunt wave 17 (finding F-W16b): the in-memory channel
repository never assigned channel ids, so every created channel landed in
by_id_[0]; "RED" and "BLUE" collided and Carol joining "RED" actually resolved to
the Blue channel object (seeing Bob, not Alice).

Run: python3 tests/diff/diff_multichannel.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def carol_red_roster(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "RED")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.join_channel(b, "BLUE")
    time.sleep(0.3)
    c, _ = bc.full_login(host, port, "carol", "secret")
    roster = bc.join_channel(c, "RED", collect=20, settle=0.6)
    showusers = sorted({u for (e, u, t) in roster if e == bc.EID_SHOWUSER})
    a.close()
    b.close()
    c.close()
    return showusers


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6348)
    ap.add_argument("--v3-port", type=int, default=6448)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = carol_red_roster("127.0.0.1", args.orig_port)
        n = carol_red_roster("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: carol RED roster {o}")
        print(f"V3      : carol RED roster {n}")
        # RED must contain alice + carol, and never bob (who is in BLUE).
        ok = o == ["alice", "carol"] and n == o and "bob" not in n
        if ok:
            print("PASS: distinct channels isolated, matches oracle")
            return 0
        print("FAIL: channel isolation diverges from oracle")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
