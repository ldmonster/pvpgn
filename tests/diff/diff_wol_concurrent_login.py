#!/usr/bin/env python3
"""Differential test: WOL kick-old-login (second login kicks the first session).

WOL analog of the BNCS kick-old-login fixes (w49 OLS, w50 W3/NLS). When the same
account logs in a second time, the original server kicks the first session. v3's
WOL auth previously did (void)attach() and ignored the single-session-policy
failure, so BOTH sessions stayed alive — the old one a ghost.

    alice1: WOL login (kept open)
    alice2: WOL login (same account)   -> must kick alice1
    probe alice1: should be DISCONNECTED (server closed it)

Run: python3 tests/diff/diff_wol_concurrent_login.py
"""
import argparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _alive(c):
    """True iff c's socket is still open (probe with LIST + read)."""
    try:
        c.send_line("LIST")
    except OSError:
        return False
    c.sock.settimeout(1.0)
    try:
        for _ in range(40):
            line = c.read_line()
            if line is None:
                return False  # server closed the connection
            if wc.WolClient.numeric(line) == wc.RPL_LISTEND:
                return True
        return True
    except (socket.timeout, ConnectionResetError, OSError):
        return False


def scenario(host, port):
    a1 = wc.wol_session(host, port, "alice", "apass")
    if a1 is None:
        return {"a1_login": False, "a2_login": False, "a1_alive_after_a2": None}
    time.sleep(0.2)
    a2 = wc.wol_session(host, port, "alice", "apass")
    a2_ok = a2 is not None
    time.sleep(0.5)
    a1_alive = _alive(a1)
    try:
        a1.close()
    except OSError:
        pass
    if a2:
        try:
            a2.close()
        except OSError:
            pass
    return {"a1_login": True, "a2_login": a2_ok, "a1_alive_after_a2": a1_alive}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6378)
    ap.add_argument("--v3-port", type=int, default=6478)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        rows = [
            ("a2_login", o["a2_login"], n["a2_login"]),
            ("a1_kicked", o["a1_alive_after_a2"] is False,
             n["a1_alive_after_a2"] is False),
        ]
        print(f"{'field':<14}{'original':<14}{'v3':<14}match")
        print("-" * 50)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<14}{str(ov):<14}{str(nv):<14}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["a2_login"] and o["a1_alive_after_a2"] is False)
        if success:
            print("WOL second login kicks the first session (matches oracle).")
            return 0
        print("FAIL: WOL kick-old-login divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
