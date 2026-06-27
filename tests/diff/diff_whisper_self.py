#!/usr/bin/env python3
"""Differential test: whispering to oneself (/w <self> ...).

The original do_whisper (command.cpp) has no self-target special case: it sends
the sender's acknowledgement (message_type_whisperack -> EID_WHISPERSENT 0x0a)
FIRST, then the target's message (message_type_whisper -> EID_WHISPER 0x04).
When the target IS the sender, both lines land on the SAME connection, so the
client observes WHISPERSENT then WHISPER, in that exact order.

Regression guard for bug-hunt wave 84: v3's handle_whisper delivered the
EID_WHISPER to the target before the EID_WHISPERSENT to the sender. For a
whisper to another user this is invisible (two sockets), but a self-whisper
exposed the reversed order (0x04 before 0x0a). Reordered to match the oracle.

Run: python3 tests/diff/diff_whisper_self.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def scenario(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.enter_chat(a, "alice")
    time.sleep(0.3)
    while a.recv() is not None:
        pass
    a.send(bc.SID_CHATCOMMAND, bc.cstring("/w alice talking to myself"))
    time.sleep(0.4)
    a.sock.settimeout(1.0)
    out = []
    for _ in range(8):
        r = a.recv()
        if r is None:
            break
        if r[0] == bc.SID_CHATEVENT:
            e = bc.parse_chat_event(r[1])
            if e:
                out.append((e[0], e[1], e[2]))
    a.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wave", type=int, default=11)
    args = ap.parse_args()
    ob = 10000 + (args.wave * 40) % 40000
    vb = ob + 12

    orig = OriginalBnetd("/home/cnupt/work/pvpgn-server", ob)
    orig.start()
    v3 = V3Bnetd("/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd", vb)
    v3.start()
    try:
        o = scenario("127.0.0.1", ob)
        v = scenario("127.0.0.1", vb)
    finally:
        orig.stop()
        v3.stop()

    print("ORACLE:", o)
    print("V3    :", v)

    # Compare the (event_id, username) ordering; localized text is irrelevant.
    o_seq = [(eid, name) for (eid, name, _t) in o]
    v_seq = [(eid, name) for (eid, name, _t) in v]
    expected = [(0x0a, "alice"), (0x04, "alice")]
    ok = o_seq == expected and v_seq == expected
    print("PASS" if ok else "FAIL",
          ": self-whisper order WHISPERSENT(0x0a) then WHISPER(0x04)")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
