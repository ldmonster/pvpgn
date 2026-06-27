#!/usr/bin/env python3
"""Differential test: WOL JOIN is broadcast to existing channel members.

When a WOL/IRC client JOINs a channel that already has members, the original
server (channel_add_connection -> channel_message_log -> message_type_join)
broadcasts a JOIN line to every EXISTING member so their roster stays current in
real time. v3 previously sent the join echo, 353 NAMES, 332 TOPIC and 366 only
to the JOINER and dropped JoinChannelResult.members_to_notify — so existing
members never learned of the newcomer (a stale-roster ghost) until they manually
re-NAMES/LIST. This asymmetry was v3-internal: PART/KICK already broadcast via
route_irc_line(line, members_to_notify), JOIN did not.

Decisive observable: observer "jobs" is already in #joinb, then "jvic" joins.
  ORACLE -> jobs receives ":jvic!WCHT@127.0.0.1 JOIN #joinb"
  V3 (before fix) -> jobs receives nothing.

The source hostmask is environment-dependent (oracle WCHT@<ip> vs v3
@Battle.net), so — like diff_wol_part / diff_wol_kick — this compares the
decisive observables (presence + source nick + channel named), not the literal
prefix. It also checks the joiner still gets its own JOIN echo (self-echo) and
that the newcomer is NOT erroneously notified of its own join twice.

Run: python3 tests/diff/diff_wol_join_broadcast.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.5):
    time.sleep(settle)
    c.sock.settimeout(0.4)
    out = []
    try:
        while True:
            line = c.read_line()
            if line is None:
                break
            out.append(line)
    except OSError:
        pass
    return out


def _has_join_for(lines, nick, channel):
    """True iff some line is a JOIN naming `nick` (as source) and `channel`."""
    for ln in lines:
        if " JOIN " not in ln:
            continue
        src = ln[1:].split("!", 1)[0].split(" ", 1)[0] if ln.startswith(":") else ""
        after = ln.split(" JOIN ", 1)[1].strip()
        chan = after.split(" ", 1)[0].lstrip(":")
        if (src.lower() == nick.lower()
                and chan.lstrip("#").lower() == channel.lstrip("#").lower()):
            return True
    return False


def scenario(host, port):
    chan = "#joinb"
    obs = wc.wol_session(host, port, "jobs", "pw")
    if obs is None:
        return None
    wc.wol_join(obs, chan)
    _drain(obs, 0.6)  # fully flush observer's own join replies

    vic = wc.wol_session(host, port, "jvic", "pw")
    if vic is None:
        obs.close()
        return None
    # wol_join() reads the joiner's own replies (self echo + 353/332/366).
    vic_self = wc.wol_join(vic, chan)
    obs_saw = _drain(obs, 0.8)    # existing member must see jvic's JOIN

    obs.close()
    vic.close()
    return {
        # existing member learns of the newcomer in real time
        "member_sees_join": _has_join_for(obs_saw, "jvic", chan),
        # joiner still receives its own JOIN echo
        "joiner_self_echo": _has_join_for(vic_self, "jvic", chan),
        # joiner is NOT also broadcast its own join (no self-duplication)
        "no_self_dup_to_others": not _has_join_for(obs_saw, "jobs", chan),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6372)
    ap.add_argument("--v3-port", type=int, default=6472)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        if not o or not n:
            print("FAIL: login/setup failed")
            return 1
        fields = ["member_sees_join", "joiner_self_echo", "no_self_dup_to_others"]
        print(f"{'field':<26}{'oracle':<10}{'v3':<10}match")
        print("-" * 54)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<26}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        expected = all(o[f] for f in fields)
        if all_ok and expected:
            print("WOL JOIN matches the oracle (broadcast to existing members + "
                  "self echo, no self-duplication).")
            return 0
        print("FAIL: WOL JOIN broadcast divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
