#!/usr/bin/env python3
"""Differential test: whisper target name case canonicalization.

Account "bob" exists. Alice sends `/w BOB hi` (uppercase target). The original
builds the sender's EID_WHISPERSENT (0x0a) username from the TARGET connection's
canonical account name (conn_get_chatcharname(me, dst)), so the ack carries the
canonical 'bob' — NOT the raw-typed 'BOB'. The recipient's EID_WHISPER (0x04)
carries the sender 'alice' in both. v3 must match.

Regression guard for bug-hunt wave 139: v3 echoed the raw-typed target case
('BOB') in EID_WHISPERSENT instead of the canonical account name ('bob').

Run: python3 tests/diff/diff_whisper_case.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

EID_WHISPER = 0x04
EID_WHISPERSENT = 0x0a


def drain(c, n=8):
    c.sock.settimeout(1.0)
    out = []
    for _ in range(n):
        r = c.recv()
        if r is None:
            break
        if r[0] == bc.SID_CHATEVENT:
            e = bc.parse_chat_event(r[1])
            if e:
                out.append((e[0], e[1], e[2]))
    return out


def scenario(host, port):
    b, _ = bc.full_login(host, port, "bob", "secret")
    a, _ = bc.full_login(host, port, "alice", "secret")
    time.sleep(0.3)
    while a.recv() is not None:
        pass
    while b.recv() is not None:
        pass
    # Uppercase target whose canonical account name is lowercase "bob".
    a.send(bc.SID_CHATCOMMAND, bc.cstring("/w BOB hi"))
    time.sleep(0.4)
    res = {
        "alice": [e for e in drain(a) if e[0] == EID_WHISPERSENT],
        "bob": [e for e in drain(b) if e[0] == EID_WHISPER],
    }
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
    ap.add_argument("--orig-port", type=int, default=13840)
    ap.add_argument("--v3-port", type=int, default=13846)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(0.5)
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: {o}")
        print(f"V3      : {n}")
        # Oracle canonicalizes target case in WHISPERSENT; recipient sees 'alice'.
        ok = (
            o["alice"] == [(EID_WHISPERSENT, "bob", "hi")] and
            o["bob"] == [(EID_WHISPER, "alice", "hi")] and
            n["alice"] == o["alice"] and
            n["bob"] == o["bob"]
        )
        if ok:
            print("PASS: whisper target case canonicalization matches oracle")
            return 0
        print("FAIL: divergence in whisper target case canonicalization")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
