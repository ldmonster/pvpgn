#!/usr/bin/env python3
"""Differential test: /me emote (/emote).

Alice and Bob share a channel; Alice sends `/me waves`. The original broadcasts
EID_EMOTE (0x17, username=alice) to the whole channel INCLUDING the sender
(unlike TALK, which the server suppresses for the speaker). v3 must match.

Regression guard for bug-hunt wave 18: v3 never implemented /me — it fell through
to the generic command dispatch and was rejected as unknown, so emotes were
silently dropped; once implemented, the sender must also receive the echo.

Run: python3 tests/diff/diff_emote.py
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


def emote_scenario(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "G")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.join_channel(b, "G")
    time.sleep(0.3)
    while a.recv() is not None:
        pass
    while b.recv() is not None:
        pass
    a.send(bc.SID_CHATCOMMAND, bc.cstring("/me waves"))
    time.sleep(0.4)
    res = {"bob": _drain(b), "alice_self": _drain(a)}
    a.close()
    b.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6354)
    ap.add_argument("--v3-port", type=int, default=6454)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = emote_scenario("127.0.0.1", args.orig_port)
        n = emote_scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: {o}")
        print(f"V3      : {n}")
        want = {"bob": [(0x17, "alice", "waves")],
                "alice_self": [(0x17, "alice", "waves")]}
        if o == want and n == want:
            print("PASS: /me emote (incl. self-echo) matches oracle")
            return 0
        print("FAIL: divergence in /me emote")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
