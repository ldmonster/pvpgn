#!/usr/bin/env python3
"""Differential test: BNFTP CLIENT_FILE_REQ2 (0x0200) two-step handshake.

The War3 two-step download starts with a CLIENT_FILE_REQ2 packet. The oracle
replies with a raw 4-byte SERVER_FILE_UNKNOWN1 = 0xdeadbeef and KEEPS the
connection open (conn_state_pending_raw) to await CLIENT_FILE_REQ3, per
src/bnetd/handle_file.cpp. v3 must mirror: 4 bytes 0xdeadbeef + socket open.
"""
import os
import socket
import struct
import sys
import time

DIFF = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, DIFF)
from original_server import OriginalBnetd
from v3_server import V3Bnetd

ORACLE_REPO = "/home/cnupt/work/pvpgn-server"
V3_BIN = "/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd"
PORT_BASE = 11540  # oracle BNCS; v3 BNCS = PORT_BASE+6


def build_req2(archtag=b"68XI", clienttag=b"3RAW"):
    body = archtag + clienttag + struct.pack("<Q", 0)
    size = 4 + len(body)  # 20
    return struct.pack("<HH", size, 0x0200) + body


def probe(port):
    s = socket.create_connection(("127.0.0.1", port), timeout=4.0)
    s.settimeout(2.0)
    s.sendall(b"\x02")  # BNFTP init class byte
    s.sendall(build_req2())
    data = b""
    closed = False
    try:
        while len(data) < 4:
            ch = s.recv(4096)
            if not ch:
                closed = True
                break
            data += ch
    except socket.timeout:
        pass
    # Probe whether the socket is still open: a graceful peer-close shows up
    # as a 0-byte recv. We already read the 4 reply bytes; a short follow-up
    # recv with a tiny timeout tells us if the peer closed.
    s.settimeout(0.5)
    try:
        extra = s.recv(1)
        if extra == b"":
            closed = True
    except socket.timeout:
        pass  # still open, nothing more to send -> timeout means alive
    s.close()
    return data, closed


def main():
    orig = OriginalBnetd(ORACLE_REPO, PORT_BASE)
    v3 = V3Bnetd(V3_BIN, PORT_BASE + 6)
    try:
        orig.start()
        v3.start()
        time.sleep(0.5)
        o_data, o_closed = probe(PORT_BASE)
        v_data, v_closed = probe(PORT_BASE + 6)
    finally:
        orig.stop()
        v3.stop()

    print(f"ORACLE: recv={o_data.hex()} closed={o_closed}")
    print(f"V3    : recv={v_data.hex()} closed={v_closed}")

    expected = struct.pack("<I", 0xDEADBEEF)
    ok = True
    if o_data != expected:
        print(f"FAIL: oracle did not return 0xdeadbeef (got {o_data.hex()})")
        ok = False
    if v_data != expected:
        print(f"FAIL: v3 did not return 0xdeadbeef (got {v_data.hex()})")
        ok = False
    if o_closed:
        print("FAIL: oracle unexpectedly closed the connection")
        ok = False
    if v_closed:
        print("FAIL: v3 closed the connection (should stay open, pending_raw)")
        ok = False
    if o_data != v_data or o_closed != v_closed:
        print("FAIL: v3 and oracle diverge")
        ok = False

    if ok:
        print("PASS: v3 matches oracle (0xdeadbeef + keep-open)")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
