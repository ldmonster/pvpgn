#!/usr/bin/env python3
"""Differential test: WOL PRIVMSG trailing-text rule (461 vs 411/broadcast).

The original server derives a command's message `text` ONLY from the IRC
trailing convention (handle_irc_common.cpp:206-223): a leading-colon parameter,
or the first " :" sequence. _handle_privmsg_command (handle_irc.cpp) then
requires `numparams>=1 && text` and otherwise emits

    :<server> 461 <nick> PRIVMSG :Not enough parameters   (ERR_NEEDMOREPARAMS)

It NEVER emits 411 (No recipient given) or 412 (No text to send) for PRIVMSG and
NEVER relays a non-trailing remainder. v3 previously rolled its own parser
("everything after the first space" with an optional leading ':'), so it diverged
three ways:
  - "PRIVMSG #chan"            -> v3 sent 411 (oracle: 461)
  - "PRIVMSG #chan nocolon..." -> v3 BROADCAST the body (oracle: 461)
  - "PRIVMSG #chan a:b"        -> v3 treated the mid-token colon as text (oracle: 461)

This test pins the corrected behaviour: every no-trailing-text form yields an
identical (server-name-normalised) 461 on both servers, the properly-trailing
form yields NO numeric on either, and the non-colon form is NOT broadcast to a
second channel member.

Run: python3 tests/diff/diff_wol_privmsg.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

# (line, expect_461) — all the no-trailing-text forms must collapse to a 461;
# the proper trailing form must produce no numeric.
CASES = [
    ("PRIVMSG #Lob", True),
    ("PRIVMSG #Lob nocolon body here", True),
    ("PRIVMSG #Lob hello:world", True),
    ("PRIVMSG", True),
    ("PRIVMSG #Lob :leadingcolon msg", False),
]


def _strip_server(line):
    """Drop the leading ':<servername> ' so only the protocol-meaningful
    remainder is compared (server name legitimately differs between builds)."""
    if line.startswith(":"):
        parts = line.split(" ", 1)
        return parts[1] if len(parts) > 1 else line
    return line


def _drain(c, t=0.6):
    c.sock.settimeout(t)
    out = []
    while True:
        line = c.read_line()
        if line is None:
            break
        out.append(line)
    return out


def _probe(host, wolport, line):
    """Login fresh, JOIN #Lob, send `line`, return the normalised 461 line
    (or None if no 461 was received)."""
    c = wc.wol_session(host, wolport, "alice", "pw")
    if c is None:
        return "<login-failed>"
    try:
        c.send_line("JOIN #Lob")
        _drain(c, 0.4)
        c.send_line(line)
        for got in _drain(c, 0.6):
            if wc.WolClient.numeric(got) == 461:
                return _strip_server(got)
        return None
    finally:
        c.close()


def _probe_broadcast(host, wolport):
    """Two users in #Lob; user1 sends a non-colon-trailing PRIVMSG. The oracle
    rejects it (461) and never relays, so user2 must receive NOTHING. Returns the
    list of PRIVMSG lines user2 saw (must be empty)."""
    u1 = wc.wol_session(host, wolport, "alice", "pw")
    u2 = wc.wol_session(host, wolport, "bob", "pw")
    if u1 is None or u2 is None:
        return ["<login-failed>"]
    try:
        u1.send_line("JOIN #Lob")
        _drain(u1, 0.4)
        u2.send_line("JOIN #Lob")
        _drain(u2, 0.4)
        _drain(u1, 0.2)  # clear bob's JOIN echo on u1
        u1.send_line("PRIVMSG #Lob nocolon body here")
        return [ln for ln in _drain(u2, 0.6) if "PRIVMSG" in ln.upper()]
    finally:
        u1.close()
        u2.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6440)
    ap.add_argument("--v3-port", type=int, default=6446)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        all_ok = True
        print(f"{'line':<34}{'oracle':<40}{'v3':<40}match")
        print("-" * 120)
        for line, expect_461 in CASES:
            o = _probe("127.0.0.1", orig.wolv1_port, line)
            n = _probe("127.0.0.1", v3.wol_port, line)
            same = (o == n)
            if expect_461:
                same = same and (o is not None) and ("461" in (o or ""))
            else:
                same = same and (o is None)
            all_ok &= same
            print(f"{line:<34}{str(o):<40}{str(n):<40}"
                  f"{'OK' if same else 'DIFF'}")

        print()
        ob = _probe_broadcast("127.0.0.1", orig.wolv1_port)
        nb = _probe_broadcast("127.0.0.1", v3.wol_port)
        bcast_ok = (ob == [] and nb == [])
        all_ok &= bcast_ok
        print(f"non-colon PRIVMSG NOT relayed to channel: "
              f"oracle={ob} v3={nb} -> {'OK' if bcast_ok else 'DIFF'}")

        print()
        if all_ok:
            print("WOL PRIVMSG trailing-text rule matches the oracle.")
            return 0
        print("FAIL: WOL PRIVMSG behaviour diverges from the oracle.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
