#!/usr/bin/env python3
"""Differential test: BNFTP SERVER_FILE_REPLY mtime matches the oracle exactly.

For an identical on-disk file with the same modification time, the oracle and v3
must report the SAME `timestamp` field (a Windows FILETIME: 100-ns ticks since
1601-01-01). The oracle reads the raw POSIX mtime via stat() and runs
time_to_bnettime(sfile.st_mtime, 0) (src/bnetd/file.cpp).

Regression guard for a fixed v3 bug: v3 used to derive the mtime via the
clock-domain hack
    last_write - file_time_type::clock::now() + system_clock::now()
then system_clock::to_time_t(), which on this libstdc++ truncated one second
low — so every v3 timestamp was exactly 10^7 ticks (1.0 s) below the oracle's.
v3 now reads st_mtime directly via ::stat(), matching the oracle byte-for-byte.

The body bytes and filelen already matched (covered by diff_bnftp.py); this test
isolates the `timestamp` field across several whole-second mtimes.

Run: python3 tests/diff/diff_bnftp_mtime.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

# Whole-second mtimes (the oracle echoes st_mtime exactly; no sub-second part).
MTIMES = [1000000000, 1234567890, 1600000000, 1700000123, 1717171717]


def _ts(port, name):
    c = bn.BnftpClient("127.0.0.1", port, timeout=4.0)
    c.request_file(name)
    data, _closed = c.recv_all()
    c.close()
    return (bn.parse_reply(data) or {}).get("timestamp")


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
    content = b"hello bnftp mtime"
    try:
        orig.start()
        v3.start()

        print(f"{'mtime':>12} {'oracle ts':>22} {'v3 ts':>22} match")
        print("-" * 64)
        all_ok = True
        for mt in MTIMES:
            for d in (os.path.join(orig.home, "var", "files", "m.bin"),
                      os.path.join(v3.workdir, "m.bin")):
                with open(d, "wb") as f:
                    f.write(content)
                os.utime(d, (mt, mt))
            time.sleep(0.1)
            o_ts = _ts(args.orig_port, "m.bin")
            v_ts = _ts(args.v3_port, "m.bin")
            same = (o_ts is not None and o_ts == v_ts)
            all_ok &= same
            print(f"{mt:>12} {str(o_ts):>22} {str(v_ts):>22} "
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("BNFTP reply mtime matches the oracle exactly.")
            return 0
        print("FAIL: BNFTP mtime timestamp divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
