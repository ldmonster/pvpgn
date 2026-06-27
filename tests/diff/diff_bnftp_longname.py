#!/usr/bin/env python3
"""Differential test: BNFTP serves files with long (>128-char) names.

The original BNFTP file request handler caps the client-supplied filename at
MAX_FILENAME_STR = 2048 chars (incl. NUL; src/common/field_sizes.h:55, used by
handle_file.cpp's packet_get_str_const). v3's BnftpFsm previously hard-coded a
128-char cap in is_safe_filename (bnftp_fsm.cpp), with a comment falsely claiming
it "matches legacy MAX_FILENAME_STR". Any on-disk file whose name was 129..2047
chars long was served by the oracle but rejected by v3 with a size-0 reply.

Fix (wave 86): kMaxFilenameLen raised 128 -> 2047 to mirror the legacy cap.

This test places identically-named long files (well under the filesystem 255-byte
per-component limit, but above the old 128 cap) in both servers' file dirs and
asserts both serve the full body. A short (<=128) name remains a control.

Run: python3 tests/diff/diff_bnftp_longname.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

# Filename lengths to probe: 100 (control, <=128), then 150/200/250 (>128, but
# <255 filesystem component limit). All end in ".bin".
LENGTHS = [100, 150, 200, 250]
CONTENT = bytes((i % 251) for i in range(100))


def _name(length):
    assert length > 4
    return ("a" * (length - 4)) + ".bin"


def _fetch(port, name):
    c = bn.BnftpClient("127.0.0.1", port, timeout=4.0)
    c.request_file(name)
    data, _closed = c.recv_all()
    c.close()
    r = bn.parse_reply(data) or {}
    return r.get("filelen"), len(r.get("file_data", b""))


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
        for length in LENGTHS:
            nm = _name(length)
            with open(os.path.join(orig.home, "var", "files", nm), "wb") as f:
                f.write(CONTENT)
            with open(os.path.join(v3.workdir, nm), "wb") as f:
                f.write(CONTENT)
        time.sleep(0.3)

        print(f"{'namelen':>7} {'oracle(len,bytes)':>20} "
              f"{'v3(len,bytes)':>20} match")
        print("-" * 60)
        all_ok = True
        for length in LENGTHS:
            nm = _name(length)
            o = _fetch(args.orig_port, nm)
            v = _fetch(args.v3_port, nm)
            same = (o == v and o[0] == len(CONTENT) and o[1] == len(CONTENT))
            all_ok &= same
            print(f"{length:>7} {str(o):>20} {str(v):>20} "
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("BNFTP serves long-named files, matching the oracle.")
            return 0
        print("FAIL: BNFTP long-filename divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
