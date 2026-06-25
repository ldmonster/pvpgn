#!/usr/bin/env python3
"""Differential test: channel part on disconnect.

Alice and Bob join the same channel; Alice then drops her connection. The
original broadcasts EID_LEAVE (0x03, username=alice) to the remaining members
(Bob). v3 must do the same.

Regression guard for bug-hunt wave 16: v3 only emitted EID_LEAVE on an explicit
SID_LEAVECHAT; a disconnect tore the session down via LogoutUser, which removed
the membership but discarded the resulting notification list, so the remaining
members never learned the user had gone (a ghost in their roster).

Run: python3 tests/diff/diff_leave.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def _drain(c):
    c.sock.settimeout(1.0)
    out = []
    for _ in range(8):
        r = c.recv()
        if r is None:
            break
        if r[0] == bc.SID_CHATEVENT:
            e = bc.parse_chat_event(r[1])
            if e:
                out.append((e[0], e[1], e[2]))
    return out


def leave_on_disconnect(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "W")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.join_channel(b, "W")
    time.sleep(0.3)
    while b.recv() is not None:
        pass
    a.close()  # Alice drops her connection.
    time.sleep(0.6)
    out = _drain(b)
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
    ap.add_argument("--orig-port", type=int, default=6344)
    ap.add_argument("--v3-port", type=int, default=6444)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = leave_on_disconnect("127.0.0.1", args.orig_port)
        n = leave_on_disconnect("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: bob got {o}")
        print(f"V3      : bob got {n}")
        # Bob must see a single EID_LEAVE(0x03) for alice, matching the oracle.
        ok = o == [(0x03, "alice", "")] and n == o
        if ok:
            print("PASS: channel part-on-disconnect matches oracle")
            return 0
        print("FAIL: divergence in channel part-on-disconnect")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
