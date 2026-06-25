#!/usr/bin/env python3
"""Differential test: private whisper (/w, /msg, /whisper, /m).

Alice and Bob both log in and enter chat. Alice sends `/w bob <msg>`.
The original server delivers EID_WHISPER (0x04, username=alice) to Bob and
EID_WHISPERSENT (0x0a, username=bob) back to Alice. v3 must match.

Regression guard for bug-hunt wave 15: v3 did not implement the whisper command
family at all — `/w` fell through to the generic command dispatch and returned
"Unknown command", delivering nothing to the target.

Run: python3 tests/diff/diff_whisper.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def whisper_scenario(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.enter_chat(a, "alice")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.enter_chat(b, "bob")
    time.sleep(0.3)
    while a.recv() is not None:
        pass
    while b.recv() is not None:
        pass
    a.send(bc.SID_CHATCOMMAND, bc.cstring("/w bob hey there"))
    time.sleep(0.4)

    def drain(c):
        # Whisper events arrive together right after the command; the first
        # recv() that returns None (socket timeout) means the stream is idle.
        c.sock.settimeout(1.0)
        out = []
        for _ in range(6):
            r = c.recv()
            if r is None:
                break
            if r[0] == bc.SID_CHATEVENT:
                e = bc.parse_chat_event(r[1])
                if e:
                    out.append((e[0], e[1], e[2]))
        return out

    res = {"bob": drain(b), "alice": drain(a)}
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
    ap.add_argument("--orig-port", type=int, default=6332)
    ap.add_argument("--v3-port", type=int, default=6432)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = whisper_scenario("127.0.0.1", args.orig_port)
        n = whisper_scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: {o}")
        print(f"V3      : {n}")
        # Bob must get EID_WHISPER(0x04) from alice; Alice EID_WHISPERSENT(0x0a) to bob.
        ok = (
            o["bob"] == [(0x04, "alice", "hey there")] and
            o["alice"] == [(0x0a, "bob", "hey there")] and
            n["bob"] == o["bob"] and
            n["alice"] == o["alice"]
        )
        if ok:
            print("PASS: whisper delivery matches oracle")
            return 0
        print("FAIL: divergence in whisper delivery")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
