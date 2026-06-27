#!/usr/bin/env python3
"""Differential test: SID_CLIENTID/COMPINFO1 (0x05) and COMPINFO2 (0x1E).

The legacy/OLS retail login flow (Starcraft/Diablo pre-NLS) opens with
CLIENT_COMPINFO1 (SID 0x05). The original (_client_compinfo1) always replies
with TWO packets:
  - SERVER_COMPREPLY (SID 0x05, 16-byte body) carrying the four magic
    registration constants reg_version/reg_auth/client_id/client_token, and
  - SERVER_SESSIONKEY1 (SID 0x28, 4-byte body) carrying the connection's
    (random) session key.
The sibling CLIENT_COMPINFO2 (SID 0x1E) replies with SERVER_COMPREPLY plus
SERVER_SESSIONKEY2 (SID 0x1D, 8-byte body = sessionnum + sessionkey).

v3 previously handled both as silent no-ops (BnetFsm::on returned core::ok()
with no reply), stalling legacy clients. This guards bug-hunt wave N.

The 16-byte COMPREPLY body is fully constant, so it is compared byte-for-byte.
The session-key value is per-connection random, so only its SID and length are
asserted (structure, not value).

Run: python3 tests/diff/diff_compinfo.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
from bncs_client import BncsClient  # noqa: E402

SID_COMPINFO1 = 0x05  # also SERVER_COMPREPLY
SID_SESSIONKEY1 = 0x28
SID_COMPINFO2 = 0x1E
SID_SESSIONKEY2 = 0x1D

# Constant COMPREPLY body (reg_version, reg_auth, client_id, client_token).
COMPREPLY_BODY = (
    struct.pack("<I", 0x00000001)
    + struct.pack("<I", 0xAA8843D1)
    + struct.pack("<I", 0x001B9DDA)
    + struct.pack("<I", 0xAB69F79A)
)


def _compinfo1_body():
    body = COMPREPLY_BODY
    body += b"HOST\x00"
    body += b"user\x00"
    return body


def _compinfo2_body():
    body = struct.pack("<I", 0x00000001)  # unknown1
    body += COMPREPLY_BODY
    body += b"HOST\x00"
    body += b"user\x00"
    return body


def _collect(host, port, sid, body, expect=2):
    c = BncsClient(host, port, timeout=3.0)
    c.send(sid, body)
    pkts = []
    for _ in range(expect + 2):
        r = c.recv()
        if r is None:
            break
        pkts.append(r)
    c.close()
    return pkts


def _normalize(pkts, sessionkey_sid):
    """Reduce to a comparable shape: COMPREPLY body verbatim, session key by
    SID+length only (value is per-connection random)."""
    out = []
    for sid, body in pkts:
        if sid == SID_COMPINFO1 and len(body) == 16:
            out.append(("compreply", body.hex()))
        elif sid == sessionkey_sid:
            out.append(("sessionkey", f"sid=0x{sid:02x}", f"len={len(body)}"))
        else:
            out.append((f"other-0x{sid:02x}", f"len={len(body)}"))
    return out


def scenario(host, port):
    p1 = _collect(host, port, SID_COMPINFO1, _compinfo1_body())
    p2 = _collect(host, port, SID_COMPINFO2, _compinfo2_body())
    return {
        "compinfo1": _normalize(p1, SID_SESSIONKEY1),
        "compinfo2": _normalize(p2, SID_SESSIONKEY2),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11460)
    ap.add_argument("--v3-port", type=int, default=11466)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: {o}")
        print(f"V3      : {n}")

        expect1 = [
            ("compreply", COMPREPLY_BODY.hex()),
            ("sessionkey", f"sid=0x{SID_SESSIONKEY1:02x}", "len=4"),
        ]
        expect2 = [
            ("compreply", COMPREPLY_BODY.hex()),
            ("sessionkey", f"sid=0x{SID_SESSIONKEY2:02x}", "len=8"),
        ]
        ok = (
            o["compinfo1"] == expect1
            and o["compinfo2"] == expect2
            and n == o
        )
        if ok:
            print("PASS: COMPINFO1/COMPINFO2 reply structure matches oracle")
            return 0
        print("FAIL: divergence in COMPINFO reply")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
