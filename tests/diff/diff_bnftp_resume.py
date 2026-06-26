#!/usr/bin/env python3
"""Differential test: BNFTP resume (start_offset > 0) reply `filelen` semantics.

A BNFTP CLIENT_FILE_REQ may carry a non-zero startoffset (download resume). The
original pvpgn (src/bnetd/file.cpp file_send) sets the SERVER_FILE_REPLY.filelen
to the FULL file size from stat() and only THEN fseeks to startoffset, streaming
`filelen - startoffset` bytes. Past-EOF (startoffset >= filelen) it "keeps the
real filesize" and streams nothing.

Regression guard for a fixed v3 bug: v3's BnftpFsm put `file_size - start_offset`
into the reply's filelen (so a resume reported a too-small total and a past-EOF
request reported 0), diverging from the oracle on the wire. v3 now reports the
full file size and streams file_size - start_offset bytes, matching the oracle.

Decisive observables per offset:
  - reply.filelen   == full file size (both servers)
  - delivered bytes == max(0, full_size - offset) (both servers, identical content)

Run: python3 tests/diff/diff_bnftp_resume.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

# (file_size, start_offset) cases: a mid-file resume, exact-EOF, past-EOF, zero.
CASES = [
    (4096, 0),
    (4096, 1000),
    (4096, 4096),   # exact EOF
    (4096, 9000),   # past EOF
    (100, 37),
]


def _fetch(port, name, offset):
    c = bn.BnftpClient("127.0.0.1", port, timeout=4.0)
    c.request_file(name, startoffset=offset)
    data, _closed = c.recv_all()
    c.close()
    r = bn.parse_reply(data) or {}
    return r.get("filelen"), r.get("file_data", b"")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6396)
    ap.add_argument("--v3-port", type=int, default=6496)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        # Place identical files in both servers' file dirs (one per distinct size).
        sizes = sorted({sz for sz, _ in CASES})
        for n in sizes:
            content = bytes((i % 251) for i in range(n))
            with open(os.path.join(orig.home, "var", "files", f"r{n}.bin"), "wb") as f:
                f.write(content)
            with open(os.path.join(v3.workdir, f"r{n}.bin"), "wb") as f:
                f.write(content)
        time.sleep(0.3)

        hdr = f"{'size/off':>12} {'oracle(len,bytes)':>22} {'v3(len,bytes)':>22} match"
        print(hdr)
        print("-" * len(hdr))
        all_ok = True
        for size, off in CASES:
            name = f"r{size}.bin"
            o_len, o_data = _fetch(args.orig_port, name, off)
            v_len, v_data = _fetch(args.v3_port, name, off)
            expect_bytes = max(0, size - off)
            same = (
                o_len == v_len
                and len(o_data) == len(v_data) == expect_bytes
                and o_data == v_data
            )
            all_ok &= same
            print(f"{f'{size}/{off}':>12} {str((o_len, len(o_data))):>22} "
                  f"{str((v_len, len(v_data))):>22} {'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("BNFTP resume: reply filelen + delivered bytes match the oracle.")
            return 0
        print("FAIL: BNFTP resume divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
