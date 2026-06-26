#!/usr/bin/env python3
"""Differential test: SID_CHANNELLIST (0x0B) channel-list reply structure.

The client requests the list of available channels via CLIENT_PROGIDENT2 /
SID_CHANNELLIST (0x0B) carrying a 4-byte client tag. With a tag of 0 the
original server (`_client_progident2`, handle_bnet.cpp) replies with
SERVER_CHANNELLIST (0x0B): a sequence of NUL-terminated channel names followed
by an empty-string terminator.

The handler is registered ONLY in the logged-in table, so the request must be
sent AFTER login.

What is compared (the channel NAMES themselves legitimately differ — each
server seeds a different default channel set):

  1. Both servers answer 0x0B with a 0x0B.
  2. Both bodies are well-formed: a run of NUL-terminated strings ending in an
     empty terminator, with no trailing garbage.
  3. NEITHER list contains "The Void".  The original flags the kicked/banned
     limbo channel (channel_flags_thevoid) and explicitly excludes it from the
     channel list (and from /channels and IRC LIST).  The v3 rewrite used to
     seed "The Void" as an ordinary permanent channel and advertised it; this
     test guards the fix (domain ChannelFlag::TheVoid + ListChannels filter).

Run: python3 tests/diff/diff_channellist.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_CHANNELLIST = 0x0B


def _recv_match(client, want_sid, tries=20):
    for _ in range(tries):
        r = client.recv()
        if r is None:
            return None
        sid, body = r
        if sid == bc.SID_PING:
            client.send(bc.SID_PING, body[:4])
            continue
        if sid == want_sid:
            return body
    return None


def _parse_strlist(body):
    """Parse a NUL-terminated string list ending in an empty string.

    Returns (names, well_formed). well_formed is True iff the body ends exactly
    at the empty-string terminator (no trailing bytes after it)."""
    names = []
    i = 0
    n = len(body)
    while i < n:
        end = body.find(b"\x00", i)
        if end < 0:
            return names, False  # unterminated string
        s = body[i:end]
        i = end + 1
        if s == b"":
            # terminator: must be the end of the packet
            return names, (i == n)
        names.append(s.decode("latin-1", "replace"))
    # ran out of bytes without an explicit empty terminator
    return names, False


def scenario(host, port):
    out = {}
    c, _ = bc.full_login(host, port, "alice", "alicepass")
    c.send(SID_CHANNELLIST, struct.pack("<I", 0))  # client tag 0 => list request
    body = _recv_match(c, SID_CHANNELLIST)
    c.close()
    if body is None:
        out["reply"] = False
        return out
    names, well_formed = _parse_strlist(body)
    out["reply"] = True
    out["well_formed"] = well_formed
    out["has_the_void"] = ("The Void" in names)
    out["count_nonempty"] = len(names) > 0
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=8740)
    ap.add_argument("--v3-port", type=int, default=8760)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<18}{'original':<14}{'v3':<14}match")
        print("-" * 56)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<18}{str(o[k]):<14}{str(n[k]):<14}{'OK' if same else 'DIFF'}")
        print()

        # Sanity: the oracle must answer with a well-formed, non-empty list that
        # excludes The Void (guards the probe itself).
        oracle_ok = (o.get("reply") and o.get("well_formed")
                     and o.get("count_nonempty") and not o.get("has_the_void"))
        if not oracle_ok:
            print("FAIL: oracle did not behave as expected — probe/setup broken.")
            return 2
        if all_ok:
            print("SID_CHANNELLIST reply structure matches the oracle "
                  "(well-formed list, The Void hidden).")
            return 0
        print("FAIL: v3 diverges from the oracle on SID_CHANNELLIST.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
