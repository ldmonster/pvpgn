#!/usr/bin/env python3
"""Differential test: WOL/IRC per-line "excess flood" disconnect.

The original server's common IRC/WOL line handler enforces a per-line flood cap.
In handle_irc_common_packet (src/bnetd/handle_irc_common.cpp:336-345) it
accumulates every non-'\n' byte of a line and, once the running count exceeds
`100 + MAX_IRC_MESSAGE_LEN` (= 612), logs "excess flood" and returns -1, which
the server.cpp dispatch turns into conn_close_read(). So a single line of >= 613
chars is NEVER handled: the oracle sends zero bytes back and destroys the
connection.

v3 previously had no per-line cap (only a 2048-byte no-newline accumulation
guard), so it treated a 613..2048-char line as an ordinary (unknown) command,
emitted a 421 reply, and stayed connected.

DECISIVE OBSERVABLE (pre-login, no auth needed): send "A"*N + "\n" to the WOL
listener.
  * N = 612 (control): BOTH servers emit a 421 reply line.
  * N = 613 (decisive): BOTH servers send zero reply bytes and drop the
    connection (a following PING gets no PONG; the socket is closed).

We compare STRUCTURE only (the 421 numeric code's presence for the control case;
zero-bytes + closed for the flood case), not localized reply text or server
names, per the harness normalization rules.

Run: python3 tests/diff/diff_wol_flood.py
"""
import argparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402


def probe(host, port, n):
    """Send a single N-char line + LF, read the immediate reply, then probe
    whether the connection was dropped by writing a PING and checking for EOF.

    Returns (reply_bytes, closed)."""
    s = socket.create_connection((host, port), timeout=4.0)
    s.settimeout(1.5)
    s.sendall(b"A" * n + b"\n")
    time.sleep(0.4)
    data = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    closed = False
    try:
        s.sendall(b"PING z\r\n")
        time.sleep(0.2)
        s.setblocking(False)
        try:
            extra = s.recv(65536)
            if extra == b"":
                closed = True
            else:
                data += extra
        except BlockingIOError:
            closed = False
    except OSError:
        closed = True
    finally:
        try:
            s.close()
        except OSError:
            pass
    return data, closed


def scenario(host, port):
    result = {}
    # Control: 612-char line -> a 421 reply line on both servers.
    ctrl_data, _ = probe(host, port, 612)
    result["control-612-has-421"] = (b"421" in ctrl_data)
    # Decisive: 613-char line -> zero reply bytes AND a dropped connection.
    flood_data, flood_closed = probe(host, port, 613)
    result["flood-613-zero-bytes"] = (len(flood_data) == 0)
    result["flood-613-closed"] = flood_closed
    # A clearly-over-cap line (700) behaves the same: flood + close.
    flood2_data, flood2_closed = probe(host, port, 700)
    result["flood-700-zero-bytes"] = (len(flood2_data) == 0)
    result["flood-700-closed"] = flood2_closed
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6392)
    ap.add_argument("--v3-port", type=int, default=6492)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(0.4)
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        fields = list(o.keys())
        print(f"{'field':<26}{'oracle':<10}{'v3':<10}match")
        print("-" * 54)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<26}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must actually exhibit the faithful behavior, and v3 must match.
        expected = all(o[f] for f in fields)
        if all_ok and expected:
            print("WOL excess-flood disconnect matches the oracle "
                  "(421 at 612; zero bytes + closed at >= 613).")
            return 0
        print("FAIL: WOL excess-flood divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
