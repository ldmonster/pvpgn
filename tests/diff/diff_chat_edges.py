#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Differential test: channel-talk EDGE cases (empty body, over-long body).

Probes two edge inputs that the original handles specially in
src/bnetd/handle_bnet.cpp (_client_message) + src/bnetd/message.cpp
(message_format):

  1. EMPTY body  -> the original replaces empty text with a single space
     ("empty messages crash some clients", message.cpp:993-994) and STILL
     broadcasts an EID_TALK to the rest of the channel. The sender receives
     nothing.

  2. OVER-LONG body (> MAX_MESSAGE_LEN, 255 bytes) -> packet_get_str_const
     returns NULL, the handler returns -1, and NOTHING is sent to anyone (a
     SILENT discard). The sender receives nothing.

In NEITHER case does the original send a synthetic "Invalid chat message"
notice back to the sender. Before bug-hunt wave 76, v3 rejected both inputs at
the ChatMessage::create boundary and replied with an EID_INFO "Invalid chat
message" (and dropped the empty broadcast entirely).

We compare the decisive observables: (a) what the OTHER channel member (bob)
receives, and (b) whether the SENDER (alice) gets any reply. Lengths 224..255
are intentionally NOT tested: the original gates those through flood/quota
(config + rate dependent), which v3 does not implement — not cleanly diffable.

Run: python3 tests/diff/diff_chat_edges.py
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
    old = c.sock.gettimeout()
    c.sock.settimeout(0.3)
    try:
        while c.recv() is not None:
            pass
    finally:
        c.sock.settimeout(old)


def _collect(c, settle=0.4):
    """Return [(eid, user, text)] for the chat events received.

    Uses a short socket timeout so an idle connection (the common case here —
    silent discard / no sender reply) returns promptly instead of blocking on
    the default 5s recv timeout.
    """
    time.sleep(settle)
    old = c.sock.gettimeout()
    c.sock.settimeout(0.4)
    out = []
    try:
        for _ in range(8):
            r = c.recv()
            if r is None:
                break
            if r[0] == bc.SID_CHATEVENT:
                ev = bc.parse_chat_event(r[1])
                if ev:
                    out.append((ev[0], ev[1], ev[2]))
    finally:
        c.sock.settimeout(old)
    return out


def scenario(host, port, body):
    """alice sends `body`; report what bob sees and whether alice gets a reply."""
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "PvPGN")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.join_channel(b, "PvPGN")
    time.sleep(0.3)
    _drain(a)
    _drain(b)
    a.send(bc.SID_CHATCOMMAND, bc.cstring(body))
    bob_talk = [e for e in _collect(b) if e[0] == 0x05]
    alice_any = _collect(a)
    a.close()
    b.close()
    return {"bob_talk": bob_talk, "alice_any": alice_any}


def run(host, port):
    return {
        "empty": scenario(host, port, ""),
        "overlong": scenario(host, port, "x" * 300),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6360)
    ap.add_argument("--v3-port", type=int, default=6460)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    rc = 0
    try:
        orig.start()
        v3.start()
        o = run("127.0.0.1", args.orig_port)
        n = run("127.0.0.1", args.v3_port)
    finally:
        v3.stop()
        orig.stop()

    print(f"ORIGINAL: {o}")
    print(f"V3      : {n}")

    # --- empty: bob gets exactly one TALK from alice with a single-space body;
    #     alice (sender) gets nothing. ---
    def check(label, ov, nv):
        nonlocal rc
        if ov == nv:
            print(f"PASS[{label}]: matches oracle -> {nv}")
        else:
            print(f"FAIL[{label}]: oracle={ov} v3={nv}")
            rc = 1

    # empty: structural expectations on top of the oracle match.
    eo, en = o["empty"], n["empty"]
    check("empty.bob_talk", eo["bob_talk"], en["bob_talk"])
    check("empty.alice_reply", eo["alice_any"], en["alice_any"])
    # explicit shape: exactly one TALK, body == " ", no sender reply
    if not (len(en["bob_talk"]) == 1 and en["bob_talk"][0][0] == 0x05
            and en["bob_talk"][0][2] == " " and en["alice_any"] == []):
        print("FAIL[empty.shape]: v3 empty-message handling not faithful")
        rc = 1

    # overlong: nobody receives anything (silent discard), no sender reply.
    oo, no = o["overlong"], n["overlong"]
    check("overlong.bob_talk", oo["bob_talk"], no["bob_talk"])
    check("overlong.alice_reply", oo["alice_any"], no["alice_any"])
    if not (no["bob_talk"] == [] and no["alice_any"] == []):
        print("FAIL[overlong.shape]: v3 over-long message not silently discarded")
        rc = 1

    print("PASS: chat edge cases match oracle" if rc == 0 else "FAIL")
    return rc


if __name__ == "__main__":
    sys.exit(main())
