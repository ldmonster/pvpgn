#!/usr/bin/env python3
"""Differential test: WOL ladder verbs destroy the connection.

The original pvpgn-server's WOL ladder-server command handlers
(handle_wol.cpp) close (destroy) the client connection in the
backend-independent cases below, whereas v3 previously silently accepted them
(wol_known[] no-op) and stayed connected:

  - HIGHSCORE  : body fully commented out; UNCONDITIONALLY destroys.
  - LISTSEARCH : missing first param or trailing text -> WARN + destroy.
  - RUNGSEARCH : fewer than 4 params -> WARN + destroy.

Decisive observable: on a logged-in WOL connection, after a bare
HIGHSCORE/LISTSEARCH/RUNGSEARCH the socket is closed -> a following
"PING aliveprobe" gets NO PONG. A control connection that only sends
PING/TIME/PING stays alive on both servers (proving the aliveness probe is
real, not a harness artifact).

This test asserts BOTH servers drop the connection after each bare ladder verb,
and that BOTH keep the control connection alive.

Run: python3 tests/diff/diff_wol_ladder_destroy.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

LADDER_VERBS = ["HIGHSCORE", "LISTSEARCH", "RUNGSEARCH"]


def _alive_after(c):
    """Send a PING aliveness probe; return True iff a PONG comes back."""
    c.send_line("PING aliveprobe")
    c.sock.settimeout(2.0)
    for _ in range(10):
        line = c.read_line()
        if line is None:
            return False
        if "PONG" in line or "aliveprobe" in line:
            return True
    return False


def _drain(c, n=5):
    """Read up to n immediate reply lines (non-blocking-ish)."""
    c.sock.settimeout(1.0)
    try:
        for _ in range(n):
            if c.read_line() is None:
                break
    except Exception:
        pass
    c.sock.settimeout(5.0)


def scenario(host, port):
    out = {}
    # Each ladder verb on its own connection: bare command then aliveness probe.
    for verb in LADDER_VERBS:
        c = wc.wol_session(host, port, "lad" + verb[:3].lower(), "secretpass")
        if c is None:
            out[verb] = None
            continue
        try:
            c.send_line(verb)
            _drain(c)
            out[verb] = _alive_after(c)
        finally:
            c.close()

    # Control connection: PING/TIME/PING must keep the connection alive.
    c = wc.wol_session(host, port, "ctrluser", "secretpass")
    if c is None:
        out["control"] = None
    else:
        try:
            c.send_line("PING ctrl1")
            _drain(c)
            c.send_line("TIME")
            _drain(c)
            out["control"] = _alive_after(c)
        finally:
            c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6460)
    ap.add_argument("--v3-port", type=int, default=6470)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
    finally:
        v3.stop()
        orig.stop()

    print(f"{'case':<12}{'expect':<10}{'oracle':<10}{'v3':<10}match")
    print("-" * 50)
    ok = True
    # Ladder verbs: BOTH must drop the connection (alive == False).
    for verb in LADDER_VERBS:
        ov, nv = o[verb], n[verb]
        m = (ov is False) and (nv is False)
        ok = ok and m
        print(f"{verb:<12}{'dead':<10}{str(ov):<10}{str(nv):<10}"
              f"{'OK' if m else 'DIFF'}")
    # Control: BOTH must stay alive (alive == True).
    om, nm = o["control"], n["control"]
    cm = (om is True) and (nm is True)
    ok = ok and cm
    print(f"{'control':<12}{'alive':<10}{str(om):<10}{str(nm):<10}"
          f"{'OK' if cm else 'DIFF'}")
    print()

    if ok:
        print("WOL ladder verbs destroy the connection, matching the oracle.")
        return 0
    print("FAIL: WOL ladder-verb destroy divergence.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
