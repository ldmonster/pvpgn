#!/usr/bin/env python3
"""Differential guard: WOL PING -> PONG wire format.

The original (irc.cpp _handle_ping_command + irc_send_pong) answers a client
PING with ":<server> PONG <server>[ :<token>]" — the server hostname appears
BOTH as the message source and as the first PONG parameter, and a token is
appended as a trailing parameter only when the client supplied one. A bare
"PING" gets ":<server> PONG <server>" with NO trailing colon.

v3 previously replied "PONG :<token>" (and "PONG :" for a bare PING): it
dropped the ":<server> " source prefix, omitted the server-name parameter, and
emitted a stray empty ":" trailing for a token-less PING. This pins the fix.

Server names differ between the two servers (pvpgn.v3 vs the oracle's
hostname), so we normalize the server token to "<S>" before comparing.

Run: python3 tests/diff/diff_wol_ping.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _collect(c, t=0.4):
    c.sock.settimeout(t)
    out = []
    try:
        while True:
            line = c.read_line()
            if line is None:
                break
            out.append(line)
    except Exception:
        pass
    return out


def _normalize_pong(line, server_name):
    """Replace the server name token(s) with <S> so the two servers compare."""
    return line.replace(server_name, "<S>")


CASES = [
    ("bare PING", "PING"),
    ("PING :token", "PING :12345"),
    ("PING token", "PING abctoken"),
]


def probe(host, wolport, server_name):
    res = {}
    c = wc.wol_session(host, wolport, "pingprobe", "pw")
    if c is None:
        return None
    _collect(c, 0.3)
    for name, line in CASES:
        c.send_line(line)
        got = [_normalize_pong(l, server_name)
               for l in _collect(c, 0.35) if " PONG " in l or l.startswith("PONG")]
        res[name] = got
    c.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6420)
    ap.add_argument("--v3-port", type=int, default=6520)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        # Discover each server's hostname from its own PONG echo so we can
        # normalize. The first PONG param IS the server name.
        co = wc.wol_session("127.0.0.1", orig.wolv1_port, "namep", "pw")
        co.send_line("PING")
        oname = None
        for l in _collect(co, 0.4):
            if " PONG " in l:
                # ":<host> PONG <host>"
                oname = l.split(" PONG ", 1)[1].strip()
                break
        co.close()
        cv = wc.wol_session("127.0.0.1", v3.wol_port, "namep", "pw")
        cv.send_line("PING")
        vname = None
        for l in _collect(cv, 0.4):
            if " PONG " in l:
                vname = l.split(" PONG ", 1)[1].strip()
                break
        cv.close()

        if not oname or not vname:
            print(f"FAIL: could not determine server names oracle={oname!r} v3={vname!r}")
            return 1

        o = probe("127.0.0.1", orig.wolv1_port, oname)
        n = probe("127.0.0.1", v3.wol_port, vname)
        if o is None or n is None:
            print(f"FAIL: login failed oracle={o is None} v3={n is None}")
            return 1

        all_ok = True
        print(f"{'case':<16}{'oracle':<28}{'v3':<28}match")
        print("-" * 80)
        for name, _ in CASES:
            ov, nv = o.get(name), n.get(name)
            same = ov == nv
            all_ok &= same
            print(f"{name:<16}{str(ov):<28}{str(nv):<28}{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("PASS: WOL PONG wire format matches the oracle.")
            return 0
        print("FAIL: WOL PONG wire format diverges.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
