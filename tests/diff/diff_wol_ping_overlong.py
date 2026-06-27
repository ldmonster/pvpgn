#!/usr/bin/env python3
"""Differential test: WOL PING with an over-length token.

The original irc_send_pong (src/bnetd/irc.cpp) applies a MAX_IRC_MESSAGE_LEN
(512) guard: when the fully constructed PONG reply line would exceed 512 bytes
it logs "max message length exceeded" and sends NOTHING. v3's WolFsm::on_ping
must mirror this — an oversized PING token must suppress the PONG entirely
rather than emit an over-length line.

Decisive, hostname-independent case: a 600-char token. v3's own constructed
PONG (~627B) clearly exceeds 512 regardless of hostname, and the oracle's does
too, so BOTH must reply with 0 bytes. A short token (10 chars) is the control:
both must reply normally. (The 480-char boundary is hostname-length dependent
and therefore intentionally not asserted for cross-server equality.)
"""
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd

V3BIN = "/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd"
ORIG_REPO = "/home/cnupt/work/pvpgn-server"
PORT_BASE = 11240


def probe(host, port, token):
    s = socket.create_connection((host, port), timeout=4.0)
    s.settimeout(2.0)
    s.sendall(("PING " + token + "\r\n").encode())
    time.sleep(0.3)
    data = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    s.close()
    return data


def main():
    orig = OriginalBnetd(ORIG_REPO, PORT_BASE)
    v3 = V3Bnetd(V3BIN, PORT_BASE + 6)
    failures = []
    try:
        orig.start()
        v3.start()
        time.sleep(0.3)

        # Control: short token -> both reply with a PONG line.
        o_short = probe("127.0.0.1", orig.wolv1_port, "x" * 10)
        n_short = probe("127.0.0.1", v3.wol_port, "x" * 10)
        if b"PONG" not in o_short:
            failures.append(f"control: oracle did not PONG short token: {o_short!r}")
        if b"PONG" not in n_short:
            failures.append(f"control: v3 did not PONG short token: {n_short!r}")

        # Decisive: 600-char token -> BOTH suppress (0 bytes).
        o_big = probe("127.0.0.1", orig.wolv1_port, "x" * 600)
        n_big = probe("127.0.0.1", v3.wol_port, "x" * 600)
        if len(o_big) != 0:
            failures.append(f"oracle replied to 600-char PING ({len(o_big)} bytes): {o_big[:80]!r}")
        if len(n_big) != 0:
            failures.append(f"v3 replied to 600-char PING ({len(n_big)} bytes): {n_big[:80]!r}")

        print(f"control short: oracle={len(o_short)}B v3={len(n_short)}B (both PONG)")
        print(f"decisive 600 : oracle={len(o_big)}B v3={len(n_big)}B (both suppress)")
    finally:
        v3.stop()
        orig.stop()

    if failures:
        print("FAIL")
        for f in failures:
            print("  -", f)
        sys.exit(1)
    print("PASS: v3 mirrors oracle PONG 512-byte length guard")


if __name__ == "__main__":
    main()
