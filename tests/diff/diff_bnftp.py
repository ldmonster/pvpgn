#!/usr/bin/env python3
"""Differential test: BNFTP file transfer delivers the full file body.

BNFTP (Battle.net File Transfer) shares the bnet listener: a connection that
opens with init byte 0x02 (CLIENT_INITCONN_CLASS_FILE) requests a file via
CLIENT_FILE_REQ; the server replies with a header (filelen + echoed name + mtime)
followed by `filelen` raw data bytes.

Regression guard for a fixed v3 bug: BnftpFsm called close() synchronously right
after queueing the header+body writes, and the transport's close() tore the socket
down before the body write dispatched — so v3 delivered the header but ZERO body
bytes for every file. The transport now closes gracefully (flushes the write queue
first). This test places an identical file in both servers' file dirs and verifies
v3 returns the full body matching the oracle, across several sizes.

(v3 closes the connection after serving; the oracle lingers — a minor, accepted
behavioural difference. The decisive observable is filelen + delivered bytes.)

Run: python3 tests/diff/diff_bnftp.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

SIZES = [1, 38, 440, 4096, 65536]


def _fetch(port, name):
    c = bn.BnftpClient("127.0.0.1", port, timeout=4.0)
    c.request_file(name)
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
        # Place identical files in both servers' file dirs.
        for n in SIZES:
            content = bytes((i % 251) for i in range(n))
            with open(os.path.join(orig.home, "var", "files", f"f{n}.bin"), "wb") as f:
                f.write(content)
            with open(os.path.join(v3.workdir, f"f{n}.bin"), "wb") as f:
                f.write(content)
        time.sleep(0.3)

        print(f"{'size':>7} {'oracle(len,bytes)':>22} {'v3(len,bytes)':>22} match")
        print("-" * 60)
        all_ok = True
        for n in SIZES:
            name = f"f{n}.bin"
            o_len, o_data = _fetch(args.orig_port, name)
            v_len, v_data = _fetch(args.v3_port, name)
            # Decisive: v3 advertises the right length AND delivers that many
            # bytes, identical to the oracle's served content.
            same = (o_len == v_len == n
                    and len(o_data) == len(v_data) == n
                    and o_data == v_data)
            all_ok &= same
            print(f"{n:>7} {str((o_len, len(o_data))):>22} "
                  f"{str((v_len, len(v_data))):>22} {'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("BNFTP delivers the full file body, matching the oracle.")
            return 0
        print("FAIL: BNFTP body-delivery divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
