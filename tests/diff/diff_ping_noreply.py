#!/usr/bin/env python3
"""Differential test: SID_PING (0x25) inbound is CLIENT_ECHOREPLY -> no reply.

The server periodically sends SERVER_ECHOREQ (0x25ff) and the client bounces it
back as CLIENT_ECHOREPLY (also 0x25ff, SID 0x25). The original's _client_echoreply
(handle_bnet.cpp) uses the cookie only to compute round-trip latency and sends
NOTHING in response — it does not echo the cookie a second time.

Regression guard for bug-hunt wave 89: v3's BnetFsm::on(Ping) mirrored the cookie
back, producing a spurious 0x25 packet that a real client never expects. After a
clean login (with all server-initiated pings already drained/echoed), a client
that sends one SID_PING must receive zero further packets — on both servers.

Run: python3 tests/diff/diff_ping_noreply.py
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def scenario(host, port):
    c, _ = bc.full_login(host, port, "pinguser", "secret")
    # Drain anything pending; echo any server-initiated ECHOREQ so the wire is
    # quiet before we probe.
    c.sock.settimeout(0.4)
    while True:
        r = c.recv()
        if r is None:
            break
        if r[0] == bc.SID_PING:
            c.send(bc.SID_PING, r[1][:4])
    # Now send our own ECHOREPLY; the server must NOT answer.
    c.send(bc.SID_PING, struct.pack("<I", 0x11223344))
    time.sleep(0.4)
    c.sock.settimeout(0.6)
    out = []
    while True:
        r = c.recv()
        if r is None:
            break
        out.append((r[0], r[1].hex()))
    c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wave", type=int, default=16)
    args = ap.parse_args()
    ob = 10000 + (args.wave * 40) % 40000
    vb = ob + 12

    orig = OriginalBnetd("/home/cnupt/work/pvpgn-server", ob)
    orig.start()
    v3 = V3Bnetd("/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd", vb)
    v3.start()
    try:
        o = scenario("127.0.0.1", ob)
        v = scenario("127.0.0.1", vb)
    finally:
        orig.stop()
        v3.stop()

    print("ORACLE:", o)
    print("V3    :", v)

    ok = o == [] and v == []
    print("PASS" if ok else "FAIL",
          ": CLIENT_ECHOREPLY (SID_PING 0x25) yields no server reply")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
