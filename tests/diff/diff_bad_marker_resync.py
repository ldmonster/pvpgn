#!/usr/bin/env python3
"""Differential test: BNCS bad-marker packet must not wedge the framer.

The original server's t_bnet_header is {bn_short type; bn_short size} and it
NEVER validates the 0xFF marker — a non-0xFF leading byte merely yields an
unmatched packet type. It frames purely by the 16-bit size field: an unknown /
undecodable packet is consumed by its declared size, logged+ignored, and the
stream resyncs to the next packet. The connection keeps serving.

v3 previously required marker==0xFF and, on header-parse failure, did a bare
`break` WITHOUT consuming the bad bytes or closing — so a single malformed
packet (any leading byte != 0xFF with a sane 16-bit size) sat at the head of
the buffer forever and permanently wedged ALL further packet processing on that
connection (socket stayed open, no replies ever again). A remote, unauth-able
framing-desync DoS.

Decisive observable (post-login, same socket): inject `00 25 08 00 AA AA AA AA`
(marker=0x00, declared size=8), then send a valid SID_FRIENDSLIST (FF 65 04 00).
The oracle still replies 0x65 to the follow-up (and a 3rd one); the connection
stays open. This asserts v3 now matches: drops the unknown packet by its size,
resyncs, and keeps answering.

Run: python3 tests/diff/diff_bad_marker_resync.py
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


def _collect(c, wait=0.6):
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


def _sids(data):
    """Decode a stream of BNCS packets into a list of (sid, payload_len)."""
    out = []
    i = 0
    while i + 4 <= len(data):
        marker, sid, ln = struct.unpack_from("<BBH", data, i)
        if marker != 0xFF or ln < 4 or i + ln > len(data):
            out.append(("RAW@%d" % i, data[i:].hex()))
            break
        out.append((hex(sid), ln - 4))
        i += ln
    return out


def _login(host, port):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    bc.create_account_ols(c, "bmuser", "pw")
    bc.login_ols(c, "bmuser", "pw", ctok, stok)
    bc.enter_chat(c, "bmuser")
    _collect(c, 0.4)
    return c


def scenario(host, port):
    c = _login(host, port)
    # Baseline: a valid FRIENDSLIST must elicit a 0x65 reply.
    c.sock.sendall(struct.pack("<BBH", 0xFF, 0x65, 4))
    baseline = _sids(_collect(c, 0.5))

    # Inject a bad-marker packet (marker=0x00, declared size=8) then a valid
    # FRIENDSLIST. A correctly-resyncing server still answers the FRIENDSLIST.
    bad = struct.pack("<BBH", 0x00, 0x25, 8) + b"\xAA\xAA\xAA\xAA"
    c.sock.sendall(bad)
    time.sleep(0.2)
    c.sock.sendall(struct.pack("<BBH", 0xFF, 0x65, 4))
    after = _sids(_collect(c, 0.7))

    # A 3rd valid packet must also still be answered.
    closed = False
    third = []
    try:
        c.sock.settimeout(0.5)
        c.sock.sendall(struct.pack("<BBH", 0xFF, 0x65, 4))
        third = _sids(_collect(c, 0.5))
    except OSError:
        closed = True
    c.close()

    def _replied(pkts):
        return any(isinstance(s, str) and s == "0x65" for s, _ in pkts)

    return {
        "baseline_friendslist_reply": _replied(baseline),
        "reply_after_bad_marker": _replied(after),
        "reply_to_third_packet": _replied(third),
        "socket_stayed_open": not closed,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11580)
    ap.add_argument("--v3-port", type=int, default=11586)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(1.0)
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        fields = ["baseline_friendslist_reply", "reply_after_bad_marker",
                  "reply_to_third_packet", "socket_stayed_open"]
        print(f"{'field':<30}{'oracle':<10}{'v3':<10}match")
        print("-" * 58)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<30}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must actually resync (answer after the bad marker) and v3
        # must match it on every field.
        expected = all(o[f] for f in fields)
        if all_ok and expected:
            print("BNCS bad-marker resync matches the oracle (drop+resync, "
                  "connection stays responsive).")
            return 0
        print("FAIL: BNCS bad-marker framer divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
