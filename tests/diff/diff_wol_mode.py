#!/usr/bin/env python3
"""Differential test: WOL MODE channel-query faithfulness (403 / 472 / 368 / 324).

The original _handle_mode_command channel branch (irc.cpp) replies:
  MODE #c   (not on a channel) -> 403 ERR_NOSUCHCHANNEL "<#c> :No such channel"
  MODE #c   (on the channel)   -> 324 RPL_CHANNELMODEIS  "<#c> +tns"
  MODE #c b                    -> 368 RPL_ENDOFBANLIST (empty ban list)
  MODE #c +z (unknown char)    -> 472 ERR_UNKNOWNMODE  ":+z is unknown mode char..."
  MODE #c +b (single token)    -> 472 ERR_UNKNOWNMODE  (only literal "b" -> 368)

v3 previously echoed 324 "+tns" for ALL channel MODE queries regardless of
membership, and treated "+b" as a ban-list query (368). This validates the
membership 403 and the 472 unknown-mode-char path.

Server name is environment-dependent, so this compares numeric codes only.

Run: python3 tests/diff/diff_wol_mode.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.6):
    time.sleep(settle)
    c.sock.settimeout(0.5)
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


def _codes(lines):
    out = []
    for l in lines:
        p = l.split()
        if len(p) > 1 and p[1].isdigit():
            out.append(p[1])
    return out


def scenario(host, port):
    c = wc.wol_session(host, port, "tmu", "pw")
    if c is None:
        return None
    # Before joining any channel: a channel MODE query must be 403.
    c.send_line("MODE #X")
    off_query = _codes(_drain(c))
    c.send_line("MODE #X +z")
    off_char = _codes(_drain(c))
    # Join, then exercise the on-channel cases.
    c.send_line("JOIN #M")
    time.sleep(0.3)
    _drain(c)
    c.send_line("MODE #M")
    plain = _drain(c)
    c.send_line("MODE #M b")
    ban = _codes(_drain(c))
    c.send_line("MODE #M +z")
    unk = _codes(_drain(c))
    c.send_line("MODE #M +b")
    plus_b = _codes(_drain(c))
    c.close()
    return {
        "off_query_403":  "403" in off_query,
        "off_char_403":   "403" in off_char,
        "plain_324_tns":  any(" 324 " in l and "+tns" in l for l in plain),
        "ban_368":        "368" in ban,
        "unknown_472":    "472" in unk,
        "plus_b_472":     "472" in plus_b,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6412)
    ap.add_argument("--v3-port", type=int, default=6512)
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
        fields = ["off_query_403", "off_char_403", "plain_324_tns",
                  "ban_368", "unknown_472", "plus_b_472"]
        print(f"{'field':<18}{'oracle':<10}{'v3':<10}match")
        print("-" * 46)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<18}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        success = all_ok and all(o[f] for f in fields)
        if success:
            print("WOL MODE channel-query matches the oracle.")
            return 0
        print("FAIL: WOL MODE divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
