#!/usr/bin/env python3
"""Differential test: WOL SETCODEPAGE/GETCODEPAGE/SETLOCALE/GETLOCALE.

Covers two observables for the four codepage/locale verbs:

1. NO-PARAM 461: the original (handle_wol.cpp _handle_set/get_codepage/locale)
   replies with ERR_NEEDMOREPARAMS when called with no parameter:

       :<server> 461 <nick> <CMD> :Not enough parameters

   OBSERVED DIVERGENCE (before this wave): v3's on_setcodepage/on_getcodepage/
   on_setlocale/on_getlocale returned core::ok() silently (a stale comment
   even claimed "original: no reply without a param" — the original source
   contradicts that and sends 461). A real client awaiting the reply would
   stall. This pins the corrected 461 wire form.

2. SET-then-GET round-trip: SETCODEPAGE/SETLOCALE echo the value back
   (329/310) and a subsequent GET for our own nick reports the value we set
   in the "<nick>`<value>" payload (328/309). This confirms the param path
   still matches the oracle after the no-param fix.

The server-name prefix legitimately differs between builds and is stripped
before comparison.

Run: python3 tests/diff/diff_wol_codepage_locale.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _strip_server(line):
    if line and line.startswith(":"):
        parts = line.split(" ", 1)
        return parts[1] if len(parts) > 1 else line
    return line


def _probe_noparam(host, wolport, cmd):
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


def _probe_roundtrip(host, wolport, setcmd, getcmd, value):
    """SET <value>, capture the echo; then GET <nick>, capture the payload.

    Returns (set_line, get_line) normalised (server prefix stripped)."""
    c = wc.wol_session(host, wolport, "alice", "pw")
    if c is None:
        return ("<login-failed>", "<login-failed>")
    try:
        c.sock.settimeout(0.8)
        set_line = None
        c.send_line("%s %s" % (setcmd, value))
        for _ in range(10):
            line = c.read_line()
            if line is None:
                break
            set_line = _strip_server(line)
            break
        get_line = None
        c.send_line("%s %s" % (getcmd, "alice"))
        for _ in range(10):
            line = c.read_line()
            if line is None:
                break
            get_line = _strip_server(line)
            break
        return (set_line, get_line)
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
    ap.add_argument("--orig-port", type=int, default=6424)
    ap.add_argument("--v3-port", type=int, default=6524)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        all_ok = True

        print("== no-param 461 ==")
        print(f"{'command':<14}{'oracle':<46}{'v3':<46}match")
        print("-" * 116)
        for cmd in ["SETCODEPAGE", "GETCODEPAGE", "SETLOCALE", "GETLOCALE"]:
            o = _probe_noparam("127.0.0.1", orig.wolv1_port, cmd)
            n = _probe_noparam("127.0.0.1", v3.wol_port, cmd)
            same = (o == n)
            all_ok &= same
            print(f"{cmd:<14}{str(o):<46}{str(n):<46}"
                  f"{'OK' if same else 'DIFF'}")

        print()
        print("== set/get round-trip ==")
        cases = [("SETCODEPAGE", "GETCODEPAGE", "1252"),
                 ("SETLOCALE", "GETLOCALE", "5")]
        for setc, getc, val in cases:
            o = _probe_roundtrip("127.0.0.1", orig.wolv1_port, setc, getc, val)
            n = _probe_roundtrip("127.0.0.1", v3.wol_port, setc, getc, val)
            same = (o == n)
            all_ok &= same
            print(f"{setc:<14}set: oracle={o[0]!r}  v3={n[0]!r}  "
                  f"{'OK' if o[0]==n[0] else 'DIFF'}")
            print(f"{getc:<14}get: oracle={o[1]!r}  v3={n[1]!r}  "
                  f"{'OK' if o[1]==n[1] else 'DIFF'}")

        print()
        if all_ok:
            print("WOL codepage/locale verbs match the oracle.")
            return 0
        print("FAIL: WOL codepage/locale verbs diverge from the oracle.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
