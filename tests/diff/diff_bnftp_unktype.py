#!/usr/bin/env python3
"""Differential test: BNFTP unknown file packet type must not close the conn.

On a BNFTP connection (init byte 0x02), when the client sends a file packet
whose type is neither CLIENT_FILE_REQ (0x0100) nor CLIENT_FILE_REQ2 (0x0200),
the oracle's handle_file_packet conn_state_connected default case logs an error
but RETURNS 0, leaving the connection in conn_state_connected. The next packet
is parsed normally, so a subsequent valid CLIENT_FILE_REQ is still served
(src/bnetd/handle_file.cpp). v3 must mirror this: an unrecognized packet is
consumed but the connection stays alive, and a following valid request is served.

Decisive observable: BNFTP init 0x02, then a type=0x9999 junk packet, then a
valid CLIENT_FILE_REQ for an existing 50-byte 'probe.bin'. Oracle returns the
full 84-byte reply (34-byte SERVER_FILE_REPLY header, filelen=50, + 50 bytes).

Note: the post-serve socket-close state is the documented/accepted "v3 closes
after serving one file vs oracle lingers" diff and is intentionally NOT asserted
here — this guard checks only that a legitimate request following an
unrecognized packet is actually served (structurally identical reply).
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
import bnftp_client as bn

ORACLE_REPO = "/home/cnupt/work/pvpgn-server"
V3_BIN = "/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd"
PORT_BASE = 12640  # oracle BNCS; v3 BNCS = PORT_BASE+6


def build_unknown(typ=0x9999):
    body = b"ABCDEFGH"  # 8 bytes of junk
    size = 4 + len(body)
    return struct.pack("<HH", size, typ) + body


def probe(port):
    s = socket.create_connection(("127.0.0.1", port), timeout=4.0)
    s.settimeout(3.0)
    s.sendall(b"\x02")  # BNFTP init class byte
    s.sendall(build_unknown(0x9999))  # unrecognized file packet type
    time.sleep(0.3)
    s.sendall(bn.build_file_req("probe.bin"))  # legitimate request
    data = b""
    try:
        while len(data) < (1 << 16):
            ch = s.recv(4096)
            if not ch:
                break
            data += ch
    except socket.timeout:
        pass
    s.close()
    return data, bn.parse_reply(data)


def normalize(r):
    """Compare structure, not server-supplied timestamps."""
    if not r:
        return None
    r = dict(r)
    r.pop("timestamp", None)
    return r


def main():
    orig = OriginalBnetd(ORACLE_REPO, PORT_BASE)
    v3 = V3Bnetd(V3_BIN, PORT_BASE + 6)
    content = bytes(range(50))
    try:
        orig.start()
        v3.start()
        open(os.path.join(orig.home, "var", "files", "probe.bin"), "wb").write(content)
        open(os.path.join(v3.workdir, "probe.bin"), "wb").write(content)
        time.sleep(0.5)
        o_data, o_reply = probe(PORT_BASE)
        v_data, v_reply = probe(PORT_BASE + 6)
    finally:
        orig.stop()
        v3.stop()

    print(f"ORACLE: recv={len(o_data)} bytes reply={normalize(o_reply)}")
    print(f"V3    : recv={len(v_data)} bytes reply={normalize(v_reply)}")

    ok = True
    if not o_reply:
        print("FAIL: oracle did not serve the file after unknown packet")
        ok = False
    if not v_reply:
        print("FAIL: v3 did not serve the file after unknown packet (regression)")
        ok = False
    if o_reply and v_reply:
        if o_reply.get("file_data") != content:
            print("FAIL: oracle file body mismatch")
            ok = False
        if v_reply.get("file_data") != content:
            print("FAIL: v3 file body mismatch")
            ok = False
        if normalize(o_reply) != normalize(v_reply):
            print("FAIL: v3 and oracle reply structures diverge")
            ok = False

    if ok:
        print("PASS: both servers serve the file that follows an unknown packet type")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
