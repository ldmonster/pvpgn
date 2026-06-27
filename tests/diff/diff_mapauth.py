#!/usr/bin/env python3
"""Differential test: SID_MAPAUTHREQ1 (0x32) / SID_MAPAUTHREQ2 (0x3C) replies.

The original (_client_mapauthreq1 / _client_mapauthreq2, handle_bnet.cpp) ALWAYS
answers a map-auth request with SERVER_MAPAUTHREPLY1/2 (same opcode 0x32 / 0x3C)
carrying a single u32 response. For a logged-in client that is NOT in a game the
server sets response = SERVER_MAPAUTHREPLY1_NO (0).

This guards the fix that made v3 reply to both opcodes (previously a pure no-op):
both servers must answer 0x32 and 0x3C with a >=4-byte body and response == 0.

Run: python3 tests/diff/diff_mapauth.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402
from bncs_client import full_login  # noqa: E402

SID_MAPAUTHREQ1 = 0x32
SID_MAPAUTHREQ2 = 0x3C


def recv_match(client, want_sid, tries=25):
    """Drain until the wanted opcode arrives, echoing SID_PING keepalives."""
    for _ in range(tries):
        r = client.recv()
        if r is None:
            return None
        sid, body = r
        if sid == bc.SID_PING:
            client.send(bc.SID_PING, body[:4])
            continue
        if sid == want_sid:
            return body
    return None


def scenario(host, port, tag):
    out = {}
    c, _ = full_login(host, port, tag, "pw")

    # CLIENT_MAPAUTHREQ1: 5 x u32 file_checksum + mapname cstring.
    c.send(SID_MAPAUTHREQ1, struct.pack("<5I", 1, 2, 3, 4, 5) + b"test.scm\x00")
    rb = recv_match(c, SID_MAPAUTHREQ1)
    out["req1_reply"] = rb is not None
    out["req1_ge4"] = rb is not None and len(rb) >= 4
    out["req1_val"] = struct.unpack_from("<I", rb, 0)[0] if rb and len(rb) >= 4 else None

    # CLIENT_MAPAUTHREQ2: u32 unknown + 5 x u32 file_hash + mapname cstring.
    c.send(SID_MAPAUTHREQ2, struct.pack("<6I", 9, 1, 2, 3, 4, 5) + b"test2.scm\x00")
    rb2 = recv_match(c, SID_MAPAUTHREQ2)
    out["req2_reply"] = rb2 is not None
    out["req2_ge4"] = rb2 is not None and len(rb2) >= 4
    out["req2_val"] = struct.unpack_from("<I", rb2, 0)[0] if rb2 and len(rb2) >= 4 else None

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
    ap.add_argument("--orig-port", type=int, default=11680)
    ap.add_argument("--v3-port", type=int, default=11686)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port, "mauo")
        n = scenario("127.0.0.1", args.v3_port, "mauv")

        keys = ["req1_reply", "req1_ge4", "req1_val",
                "req2_reply", "req2_ge4", "req2_val"]
        print(f"{'field':<14}{'oracle':<14}{'v3':<14}match")
        print("-" * 56)
        all_ok = True
        for k in keys:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<14}{str(o[k]):<14}{str(n[k]):<14}{'OK' if same else 'DIFF'}")

        # Decisive: both servers reply to both opcodes with a >=4-byte body
        # whose u32 response == 0 (not-in-a-game NO path).
        correct = all(
            o[r] and n[r] for r in ("req1_reply", "req1_ge4", "req2_reply", "req2_ge4"))
        zero = o["req1_val"] == 0 and n["req1_val"] == 0 \
            and o["req2_val"] == 0 and n["req2_val"] == 0
        print()
        if all_ok and correct and zero:
            print("MAPAUTHREQ1/2 replies match the oracle "
                  "(both answer 0x32/0x3C, response == 0).")
            return 0
        print("FAIL: divergence in MAPAUTHREQ1/2 replies.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
