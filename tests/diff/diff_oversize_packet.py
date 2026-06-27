#!/usr/bin/env python3
"""Differential test: a BNCS packet declaring size > MAX_PACKET_SIZE closes it.

The original server's packet_get_size() (common/packet.cpp) returns 0 for any
bnet packet whose declared 16-bit size exceeds MAX_PACKET_SIZE (3072, defined in
common/field_sizes.h). In net_recv_packet() the freshly-read header then yields
total_size == 0 < header_size (4) -> "corrupted packet received (closing
connection)" -> the connection is destroyed RIGHT after the header, before the
body is read. Sizes <= 3072 are accepted and the connection keeps serving.

v3 previously had no upper cap: it would await/decode bodies up to 65535 and the
session stayed fully open. This pins that v3 now mirrors the oracle exactly at
the 3072 boundary.

Decisive observable (post-login, same socket): confirm a valid SID_FRIENDSLIST
(0x65) reply, inject ONE packet whose header declares N bytes (no body sent, as
the oracle closes before the body), then re-probe FRIENDSLIST.
  N <= 3072 -> both servers stay alive and answer 0x65 again.
  N >  3072 -> both servers close (no further 0x65; socket EOF).

Run: python3 tests/diff/diff_oversize_packet.py
"""
import argparse
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def _collect(c, wait=0.5):
    time.sleep(wait)
    c.sock.settimeout(0.5)
    data = b""
    try:
        while True:
            ch = c.sock.recv(4096)
            if not ch:
                break
            data += ch
    except (socket.timeout, OSError):
        pass
    return data


def _replied_65(data):
    i = 0
    while i + 4 <= len(data):
        marker, sid, ln = struct.unpack_from("<BBH", data, i)
        if marker != 0xFF or ln < 4 or i + ln > len(data):
            break
        if sid == 0x65:
            return True
        i += ln
    return False


def _login(host, port, user):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    bc.create_account_ols(c, user, "pw")
    bc.login_ols(c, user, "pw", ctok, stok)
    bc.enter_chat(c, user)
    _collect(c, 0.4)
    return c


def _alive(host, port, user, declared_size):
    """Login, prove liveness, inject one oversize-header packet, re-probe."""
    c = _login(host, port, user)
    # Baseline liveness.
    c.sock.sendall(struct.pack("<BBH", 0xFF, 0x65, 4))
    if not _replied_65(_collect(c, 0.5)):
        c.close()
        return None  # baseline failed — test setup problem
    # Inject one complete packet declaring `declared_size` bytes (header + full
    # body). For sizes <= 3072 the oracle treats it as an unknown-type packet
    # and ignores it (staying alive); for sizes > 3072 packet_get_size()
    # returns 0 and the oracle closes right after the header. We still send the
    # full body so that, for the accepted sizes, the follow-up FRIENDSLIST is
    # NOT mis-framed as this packet's body.
    body = b"\x00" * (declared_size - 4)
    try:
        c.sock.sendall(struct.pack("<BBH", 0xFF, 0x70, declared_size) + body)
    except OSError:
        pass  # server may close mid-send for oversize packets
    time.sleep(0.3)
    # Re-probe liveness.
    after_alive = False
    try:
        c.sock.sendall(struct.pack("<BBH", 0xFF, 0x65, 4))
        after_alive = _replied_65(_collect(c, 0.5))
    except OSError:
        after_alive = False
    c.close()
    return after_alive


def scenario(host, port, tag):
    # 3072 is the inclusive max (stays alive); 3073 and above must close.
    sizes = [(100, True), (3072, True), (3073, False), (4000, False),
             (60000, False)]
    out = {}
    for idx, (sz, _) in enumerate(sizes):
        out[sz] = _alive(host, port, f"{tag}{idx}", sz)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11820)
    ap.add_argument("--v3-port", type=int, default=11826)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(1.0)
        o = scenario("127.0.0.1", args.orig_port, "orc")
        n = scenario("127.0.0.1", args.v3_port, "v3u")
        sizes = [100, 3072, 3073, 4000, 60000]
        print(f"{'declared_size':<16}{'oracle_alive':<14}{'v3_alive':<12}match")
        print("-" * 54)
        all_ok = True
        for sz in sizes:
            same = o[sz] == n[sz]
            all_ok &= same
            print(f"{sz:<16}{str(o[sz]):<14}{str(n[sz]):<12}"
                  f"{'OK' if same else 'DIFF'}")
        print()
        # Sanity: the oracle must show the documented boundary behaviour.
        oracle_ok = (o[100] is True and o[3072] is True
                     and o[3073] is False and o[4000] is False
                     and o[60000] is False)
        if all_ok and oracle_ok:
            print("BNCS oversize-packet handling matches the oracle "
                  "(close >3072, keep <=3072).")
            return 0
        print("FAIL: BNCS oversize-packet divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
