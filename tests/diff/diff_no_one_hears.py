#!/usr/bin/env python3
"""Differential test: BNCS "No one hears you." self EID_INFO.

When a BNCS client is alone in a channel (or every other member has squelched
it) and sends a normal channel message or a /me emote, the oracle replies to
the sender with a self EID_INFO (0x12, empty username) carrying the localized
"No one hears you." text. v3 must mirror this (structure only).

Cases:
  - alone talk  -> sender sees exactly one 0x12 event with empty username
  - alone /me   -> sender sees 0x17 (self echo) then 0x12 (no-one-hears)
  - two-user talk -> sender sees NO 0x12 (trigger is heard==0, not "in a chan")
"""
import sys, time
sys.path.insert(0, "/home/cnupt/work/pvpgn/tests/diff")
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc


def drain(c):
    c.sock.settimeout(0.3)
    while c.recv() is not None:
        pass


def collect(c, n=8):
    c.sock.settimeout(0.3)
    out = []
    for _ in range(n):
        r = c.recv()
        if r is None:
            break
        if r[0] == bc.SID_CHATEVENT:
            ev = bc.parse_chat_event(r[1])
            if ev:
                out.append((hex(ev[0]), ev[1]))
    return out


def alone_talk(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "SoloHearChan")
    time.sleep(0.3)
    drain(a)
    a.send(bc.SID_CHATCOMMAND, bc.cstring("hello?"))
    time.sleep(0.4)
    res = collect(a)
    a.close()
    return res


def alone_emote(host, port):
    a, _ = bc.full_login(host, port, "zoe", "secret")
    bc.join_channel(a, "SoloEmoChan")
    time.sleep(0.3)
    drain(a)
    a.send(bc.SID_CHATCOMMAND, bc.cstring("/me waves"))
    time.sleep(0.4)
    res = collect(a)
    a.close()
    return res


def two_user_talk(host, port):
    a, _ = bc.full_login(host, port, "alice", "secret")
    bc.join_channel(a, "TwoHearChan")
    b, _ = bc.full_login(host, port, "bob", "secret")
    bc.join_channel(b, "TwoHearChan")
    time.sleep(0.3)
    drain(a)
    drain(b)
    a.send(bc.SID_CHATCOMMAND, bc.cstring("hi all"))
    time.sleep(0.4)
    res = collect(a)
    a.close()
    b.close()
    return res


def run(host, port):
    return {
        "alone_talk": alone_talk(host, port),
        "alone_emote": alone_emote(host, port),
        "two_user_talk": two_user_talk(host, port),
    }


def main():
    orig = OriginalBnetd("/home/cnupt/work/pvpgn-server", 11860)
    v3 = V3Bnetd("/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd", 11866)
    try:
        orig.start()
        v3.start()
        o = run("127.0.0.1", 11860)
        n = run("127.0.0.1", 11866)
    finally:
        v3.stop()
        orig.stop()

    print("ORACLE:", o)
    print("V3    :", n)

    ok = True
    # Structural expectations (independent of localized text / server name).
    if o["alone_talk"] != [("0x12", "")]:
        print("FAIL oracle alone_talk unexpected:", o["alone_talk"]); ok = False
    if n["alone_talk"] != o["alone_talk"]:
        print("FAIL v3 alone_talk diverges:", n["alone_talk"]); ok = False

    if o["alone_emote"] != [("0x17", "zoe"), ("0x12", "")]:
        print("FAIL oracle alone_emote unexpected:", o["alone_emote"]); ok = False
    if n["alone_emote"] != o["alone_emote"]:
        print("FAIL v3 alone_emote diverges:", n["alone_emote"]); ok = False

    if o["two_user_talk"] != []:
        print("FAIL oracle two_user_talk unexpected:", o["two_user_talk"]); ok = False
    if n["two_user_talk"] != o["two_user_talk"]:
        print("FAIL v3 two_user_talk diverges:", n["two_user_talk"]); ok = False

    if ok:
        print("PASS: v3 matches oracle for No-one-hears-you EID_INFO")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
