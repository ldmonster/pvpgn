#!/usr/bin/env python3
"""Differential test: friend presence watch (login/logout whisper to mutual friends).

The original's WatchComponent::dispatch_whisper notifies a logging-in/out user's
MUTUAL, online friends with an EID_WHISPER (0x04) chat event:
    "Your friend <name> has entered <server>."   (on login)
    "Your friend <name> has left <server>."       (on logout)
fired from conn_set_account (ET_login) / conn_destroy (ET_logout), gated on
friend_get_mutual. v3 originally had no such push.

The <server> tail is each server's configured name (oracle "PvPGN Realm" vs v3
"pvpgn.v3"), so this compares the structural part: event id, originating user,
and the "Your friend X has entered/left" prefix — not the trailing server name.

Run: python3 tests/diff/diff_friends_watch.py
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


def _drain_raw(client, settle=1.3):
    time.sleep(settle)
    old = client.sock.gettimeout()
    client.sock.settimeout(0.5)
    out = []
    try:
        while True:
            r = client.recv()
            if r is None:
                break
            sid, body = r
            if sid == bc.SID_PING:
                client.send(bc.SID_PING, body[:4])
                continue
            if sid == bc.SID_CHATEVENT:
                ev = bc.parse_chat_event(body)
                if ev:
                    out.append(ev)  # (eid, user, text)
    finally:
        client.sock.settimeout(old)
    return out


def _presence(events, want_fragment):
    """Return (eid, user, normalized_prefix) for the first whisper matching."""
    for (eid, user, text) in events:
        if eid == EID_WHISPER and want_fragment in text:
            # Normalize the server-name tail: keep up to and including the verb.
            return (eid, user.lower(), want_fragment)
    return None


def scenario(host, port):
    res = {}
    a0, _ = bc.full_login(host, port, "alice", "alicepass")
    a0.close()
    time.sleep(0.4)
    b0, _ = bc.full_login(host, port, "bob", "bobpass")
    bc.friends_add(b0, "alice")
    bc.drain_chat(b0, settle=0.5)
    b0.close()
    time.sleep(0.4)

    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.friends_add(alice, "bob")  # mutual now
    bc.drain_chat(alice, settle=0.5)

    bobc, _ = bc.full_login(host, port, "bob", "bobpass")
    res["login"] = _presence(_drain_raw(alice), "has entered")

    bobc.close()
    res["logout"] = _presence(_drain_raw(alice), "has left")

    alice.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6384)
    ap.add_argument("--v3-port", type=int, default=6484)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        rows = [("login", o["login"], n["login"]), ("logout", o["logout"], n["logout"])]
        print(f"{'event':<10}{'match'}")
        print("-" * 50)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<10}{'OK' if same else 'DIFF'}")
            if not same:
                print(f"    ORACLE: {ov!r}")
                print(f"    V3    : {nv!r}")
        print()
        # Decisive: the oracle actually pushed both notices (user=bob, EID_WHISPER).
        success = (all_ok
                   and o["login"] == (EID_WHISPER, "bob", "has entered")
                   and o["logout"] == (EID_WHISPER, "bob", "has left"))
        if success:
            print("Friend presence watch matches the oracle (login + logout whisper).")
            return 0
        print("FAIL: friend presence watch divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
