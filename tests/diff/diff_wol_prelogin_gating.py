#!/usr/bin/env python3
"""Differential test: WOL pre-login command gating (con/log two-table model).

The original server (src/bnetd/handle_wol.cpp + handle_irc_common.cpp) splits WOL
verbs into two tables:

  * a "connected" table valid in ANY state (NICK, USER, PASS, PING, PONG, QUIT,
    PRIVMSG, CVERS, VERCHK, APGAR, SETOPT, SERIAL, LISTSEARCH, RUNGSEARCH,
    HIGHSCORE), and
  * a "logged in" table (LIST, TOPIC, JOIN, NAMES, PART, TIME, MODE, KICK, PAGE,
    FINDUSER, SET/GETCODEPAGE, SET/GETLOCALE, GETINSIDER, ...).

handle_irc_common tries the con-table first; if a verb is not there AND the
connection is not yet logged in, it emits exactly:

    :<srv> 421 <nick> :Unrecognized command (before login)

The 451 ERR_NOTREGISTERED numeric is *defined* by the original but NEVER sent.

This verifies v3 mirrors the model:
  * log-table verbs sent pre-login (after CVERS sets the WOL class, but before
    NICK/USER/PASS) -> 421 on BOTH servers (NOT 451, NOT executed);
  * the con-table verb SETOPT pre-login -> silently accepted (no reply) on BOTH;
  * post-login the same log-table verbs are NOT 421 on either server.

Reply targets differ by environment (oracle "UserName" vs v3 "*") and server
names/timestamps differ, so we compare the NUMERIC CODE and presence/absence,
not literal strings (per the harness normalization rules).

Run: python3 tests/diff/diff_wol_prelogin_gating.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

# Log-table verbs: all must be rejected with 421 before login.
LOG_VERBS = [
    "TIME",
    "NAMES #Lob",
    "LIST",
    "MODE #Lob",
    "PAGE a b",
    "GETCODEPAGE x",
    "SETCODEPAGE 1252",
    "GETLOCALE x",
    "SETLOCALE 1",
    "GETINSIDER x",
    "JOIN #Lob",
    "TOPIC #Lob",
    "KICK #Lob a",
    "FINDUSER x",
    "USERIP x",
]


def _drain(c, settle=0.4, t=0.4):
    time.sleep(settle)
    c.sock.settimeout(t)
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


def _prelogin_codes(host, port, verb):
    """Open a fresh conn, set WOL class via CVERS, send `verb` WITHOUT login,
    return the list of numeric codes in the replies (drops the 'before login'
    text — we compare the code only)."""
    c = wc.WolClient(host, port)
    try:
        c.send_line("CVERS 1 1000")
        _drain(c, 0.2, 0.3)  # consume any CVERS reply
        c.send_line(verb)
        lines = _drain(c, 0.3, 0.6)
        return [wc.WolClient.numeric(ln) for ln in lines
                if wc.WolClient.numeric(ln) is not None]
    finally:
        c.close()


def scenario(host, port):
    result = {}
    # --- log-table verbs pre-login -> exactly one 421, nothing else. ---
    for verb in LOG_VERBS:
        codes = _prelogin_codes(host, port, verb)
        result[f"prelogin:{verb}"] = (codes == [421])

    # --- con-table SETOPT pre-login -> silent (no reply at all). ---
    result["prelogin:SETOPT 1,1"] = (_prelogin_codes(host, port, "SETOPT 1,1") == [])

    # --- post-login: the same log-table verbs must NOT be 421. ---
    c = wc.wol_session(host, port, "gateuser", "pw")
    if c is None:
        return None
    _drain(c, 0.3)
    for verb in ["TIME", "LIST", "GETCODEPAGE gateuser"]:
        c.send_line(verb)
        codes = _drain(c, 0.3, 0.6)
        codes = [wc.WolClient.numeric(ln) for ln in codes
                 if wc.WolClient.numeric(ln) is not None]
        result[f"postlogin-not-421:{verb}"] = (421 not in codes and len(codes) > 0)
    c.close()
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6390)
    ap.add_argument("--v3-port", type=int, default=6490)
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
        fields = list(o.keys())
        print(f"{'field':<34}{'oracle':<10}{'v3':<10}match")
        print("-" * 62)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<34}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must actually exhibit the faithful behavior, and v3 must match.
        expected = all(o[f] for f in fields)
        if all_ok and expected:
            print("WOL pre-login gating matches the oracle (421 for log-table "
                  "verbs, silent SETOPT, no 421 post-login).")
            return 0
        print("FAIL: WOL pre-login gating divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
