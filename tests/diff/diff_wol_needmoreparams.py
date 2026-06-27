#!/usr/bin/env python3
"""Differential test: WOL 461 ERR_NEEDMOREPARAMS wire format.

A logged-in WOL client sends parameter-requiring commands with NO parameters.
The original pvpgn-server (handle_wol.cpp / irc_send_cmd) replies with

    :<server> 461 <nick> <CMD> :Not enough parameters

where the command name is a *middle* IRC parameter — it carries NO leading ':'.
The trailing ':' precedes only the human text "Not enough parameters".

OBSERVED DIVERGENCE (before wave 77): v3's send_numeric() unconditionally injects
" :" before its `text` argument, and the 461 call sites passed
"<CMD> :Not enough parameters" *as* that text, producing a malformed extra colon:

    :pvpgn.v3 461 alice :JOIN :Not enough parameters     (v3, WRONG)
    :host     461 alice JOIN :Not enough parameters      (oracle, correct)

That stray colon collapses "<CMD> :Not enough parameters" into a single trailing
parameter, so a real client parsing the 461 would see the command token glued to
the message text. This test pins the corrected form: it sends each command bare
to BOTH servers, captures the 461 line, normalises away the server-name prefix
(which legitimately differs), and asserts the remainder matches byte-for-byte.

Run: python3 tests/diff/diff_wol_needmoreparams.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

# Commands that require >=1 parameter and, when sent bare post-login, make the
# original emit 461. (PRIVMSG also emits 461 here but has a richer trailing-text
# rule — covered end-to-end by diff_wol_privmsg.py, so it is not duplicated in
# this pure wire-format test.)
COMMANDS = ["JOIN", "MODE", "TOPIC", "KICK", "GAMEOPT", "STARTG",
            "PAGE", "ADDBUDDY", "DELBUDDY", "GETINSIDER", "ADVERTR",
            "FINDUSER", "FINDUSEREX"]


def _strip_server(line):
    """Drop the leading ':<servername> ' so only the protocol-meaningful
    remainder (code, nick, params, trailing) is compared. Server name + host
    legitimately differ between the two builds."""
    if line.startswith(":"):
        parts = line.split(" ", 1)
        return parts[1] if len(parts) > 1 else line
    return line


def _probe(host, wolport, cmd):
    """Login fresh, send `cmd` bare, return the normalised 461 line (or None)."""
    c = wc.wol_session(host, wolport, "alice", "pw")
    if c is None:
        return "<login-failed>"
    try:
        c.sock.settimeout(0.8)
        c.send_line(cmd)
        for _ in range(10):
            line = c.read_line()
            if line is None:
                break
            if wc.WolClient.numeric(line) == 461:
                return _strip_server(line)
        return None
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6402)
    ap.add_argument("--v3-port", type=int, default=6502)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        print(f"{'command':<12}{'oracle 461 (server-stripped)':<42}"
              f"{'v3':<42}match")
        print("-" * 110)
        all_ok = True
        for cmd in COMMANDS:
            o = _probe("127.0.0.1", orig.wolv1_port, cmd)
            n = _probe("127.0.0.1", v3.wol_port, cmd)
            same = (o == n)
            all_ok &= same
            print(f"{cmd:<12}{str(o):<42}{str(n):<42}"
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("WOL 461 ERR_NEEDMOREPARAMS format matches the oracle.")
            return 0
        print("FAIL: WOL 461 format diverges from the oracle.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
