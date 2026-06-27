#!/usr/bin/env python3
"""Differential test: BNFTP CLIENT_FILE_REQ filename must be NUL-terminated.

A CLIENT_FILE_REQ carries the requested filename as a NUL-terminated string at
the tail of the packet body. The original pvpgn reads it with
packet_get_str_const (src/common/packet.cpp), which scans for a NUL within the
declared packet size and returns NULL when none is found; handle_file_packet
(src/bnetd/handle_file.cpp) then logs "missing or too long filename" and returns
-1 WITHOUT calling file_send — so the server sends NOTHING.

Regression guard for a fixed v3 bug: v3's BnftpFsm::try_dispatch used
strnlen(fname_start, fname_max) and, when no NUL was present, took ALL remaining
packet bytes as the filename and served the file — a reply a real client never
sees from the oracle. v3 now requires a NUL terminator within the packet and
sends nothing (closing gracefully) when one is absent, matching the oracle.

Decisive observable, using a real 38-byte file f38.bin:
  - control  (filename properly NUL-terminated): BOTH serve a 70-byte reply
    (32-byte SERVER_FILE_REPLY header + 38 data bytes).
  - unterminated (filename fills the packet to its end with NO trailing NUL):
    BOTH return ZERO BNFTP reply bytes (no SERVER_FILE_REPLY header).

Run: python3 tests/diff/diff_bnftp_nonul.py
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

NAME = b"f38.bin"
CONTENT = bytes((i % 251) for i in range(38))


def build_req(name_bytes, nul):
    """CLIENT_FILE_REQ; size set so the filename fills exactly to packet end."""
    fn = name_bytes + (b"\x00" if nul else b"")
    body = bn.ARCHTAG + bn.CLIENTTAG
    body += struct.pack("<III", 0, 0, 0)
    body += struct.pack("<Q", 0)
    body += fn
    size = 4 + len(body)
    return struct.pack("<HH", size, bn.CLIENT_FILE_REQ) + body


def fetch(port, pkt):
    c = bn.BnftpClient("127.0.0.1", port, timeout=2.5)
    c.send_raw(pkt)
    data, _closed = c.recv_all()
    c.close()
    return data


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6398)
    ap.add_argument("--v3-port", type=int, default=6498)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        with open(os.path.join(orig.home, "var", "files", "f38.bin"), "wb") as f:
            f.write(CONTENT)
        with open(os.path.join(v3.workdir, "f38.bin"), "wb") as f:
            f.write(CONTENT)
        time.sleep(0.4)

        cases = [
            ("NUL-terminated (control)", build_req(NAME, nul=True), 70),
            ("unterminated filename", build_req(NAME, nul=False), 0),
        ]

        hdr = f"{'case':>26} {'expect':>7} {'oracle':>7} {'v3':>5} match"
        print(hdr)
        print("-" * len(hdr))
        all_ok = True
        for label, pkt, want in cases:
            o = fetch(args.orig_port, pkt)
            v = fetch(args.v3_port, pkt)
            same = (len(o) == want and len(v) == want)
            all_ok &= same
            print(f"{label:>26} {want:>7} {len(o):>7} {len(v):>5} "
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("BNFTP filename NUL-termination: v3 matches the oracle.")
            return 0
        print("FAIL: BNFTP filename NUL-termination divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
