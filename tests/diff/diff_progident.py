#!/usr/bin/env python3
"""Differential test: CLIENT_PROGIDENT / SERVER_AUTHREQ1 (SID 0x06).

The legacy/pre-NLS login flow used by Diablo / old Starcraft / D2 clients opens
with CLIENT_PROGIDENT (SID 0x06: archtag + clienttag + versionid + unknown1).
The original (_client_progident) always replies with SERVER_AUTHREQ1 (SID 0x06):
  u64 timestamp + versioncheck filename (cstring) + checkrevision equation
  (cstring).
select_checkrevision returns a hard-coded default even with no versioncheck
config, so this is not backend-dependent.

v3 previously handled this as a silent no-op (BnetFsm::on(ProgIdent) returned
core::ok() with no reply), stalling every legacy client that blocks waiting for
the filename/equation. This guards bug-hunt wave N.

The timestamp is advisory (oracle uses file mtime) and the filename/equation
strings are localized/config-derived, so we compare STRUCTURE only: a single
SID 0x06 packet whose body parses as u64 + two NUL-terminated, non-empty
strings.

Run: python3 tests/diff/diff_progident.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
from bncs_client import BncsClient  # noqa: E402

SID_PROGIDENT = 0x06  # also SERVER_AUTHREQ1


def _progident_body():
    archtag = b"68XI"    # 'IX86' wire-packed
    clienttag = b"VD2D"  # 'D2DV' wire-packed
    return archtag + clienttag + struct.pack("<II", 3, 0)


def _collect(host, port):
    c = BncsClient(host, port, timeout=3.0)
    c.send(SID_PROGIDENT, _progident_body())
    pkts = []
    for _ in range(3):
        r = c.recv()
        if r is None:
            break
        pkts.append(r)
    c.close()
    return pkts


def _shape(pkts):
    """Reduce to a structural fingerprint: SID 0x06 with u64 + two non-empty
    NUL-terminated strings. Values (timestamp/filename/equation) are advisory."""
    if len(pkts) != 1:
        return ("badcount", len(pkts))
    sid, body = pkts[0]
    if sid != SID_PROGIDENT:
        return ("badsid", f"0x{sid:02x}")
    if len(body) < 8:
        return ("short", len(body))
    rest = body[8:]
    parts = rest.split(b"\x00")
    # rest must be: filename\0 equation\0  -> split yields [filename, equation, b""]
    if len(parts) != 3 or parts[2] != b"":
        return ("notcstrings", rest.hex())
    filename, equation = parts[0], parts[1]
    if not filename or not equation:
        return ("emptystr", (len(filename), len(equation)))
    return ("authreq1", "u64+2cstr")


def scenario(host, port):
    return _shape(_collect(host, port))


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
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: {o}")
        print(f"V3      : {n}")

        expect = ("authreq1", "u64+2cstr")
        ok = (o == expect and n == expect)
        if ok:
            print("PASS: PROGIDENT/AUTHREQ1 reply structure matches oracle")
            return 0
        print("FAIL: divergence in PROGIDENT reply")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
