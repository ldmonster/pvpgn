#!/usr/bin/env python3
"""Differential test: BNFTP request for a missing / unsafe file sends NO reply.

When a BNFTP CLIENT_FILE_REQ names a file that does not exist (or a rawname
containing a path separator), the original pvpgn (src/bnetd/file.cpp file_send)
throws inside file_get_info — stat() fails, or the '/'/'\\' guard fires — and
file_send returns -1 BEFORE pushing any packet onto the out-queue. The server
therefore sends NOTHING and simply leaves/closes the connection; the client
gets zero bytes back.

Regression guard for a fixed v3 bug: v3's BnftpFsm::handle_file_request used to
emit a phantom zero-length SERVER_FILE_REPLY header (filelen=0, echoed filename)
for both the not-found and unsafe-filename cases — a reply a real client never
sees from the oracle. v3 now sends nothing for these cases, matching the oracle.

Decisive observable per case: the server returns ZERO BNFTP reply bytes (no
SERVER_FILE_REPLY header). (v3 then closes; the oracle lingers — the same minor,
accepted behavioural difference already documented in diff_bnftp.py. The wire
observable that matters is the absence of any reply packet.)

Run: python3 tests/diff/diff_bnftp_missing.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

# Names that must NOT resolve to a real file on either server.
CASES = [
    "does_not_exist_pvpgn_xyz.bin",   # plain missing file
    "../etc/passwd",                  # path traversal ('/')
    "..\\windows\\system32\\cmd.exe",  # path traversal ('\\')
    "subdir/also_missing.bin",        # contains '/'
]


def _fetch(port, name):
    c = bn.BnftpClient("127.0.0.1", port, timeout=2.5)
    c.request_file(name)
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
    ap.add_argument("--orig-port", type=int, default=6396)
    ap.add_argument("--v3-port", type=int, default=6496)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(0.3)

        hdr = f"{'request':>34} {'oracle bytes':>14} {'v3 bytes':>10} match"
        print(hdr)
        print("-" * len(hdr))
        all_ok = True
        for name in CASES:
            o = _fetch(args.orig_port, name)
            v = _fetch(args.v3_port, name)
            # Decisive: BOTH return zero BNFTP reply bytes (no header packet).
            same = (len(o) == 0 and len(v) == 0)
            all_ok &= same
            print(f"{name:>34} {len(o):>14} {len(v):>10} "
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("BNFTP missing/unsafe file: no reply, matching the oracle.")
            return 0
        print("FAIL: BNFTP missing/unsafe-file reply divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
