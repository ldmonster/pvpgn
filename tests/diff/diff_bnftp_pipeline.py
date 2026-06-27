#!/usr/bin/env python3
"""Differential test: two BNFTP CLIENT_FILE_REQ packets in one TCP segment.

Regression guard for a use-after-erase bug: BnftpFsm copied the filename out of
the receive buffer AFTER erasing the consumed packet, so when a second request
was pipelined in the same segment, the memmove'd bytes corrupted the first
request's name — v3 served the wrong file (or nothing). The fix materializes the
filename before the erase. Both files are placed identically in both servers'
file dirs; we send req("a.bin")+req("longname.dat") in ONE sendall and check the
first reply carries a.bin's length.

Run: python3 tests/diff/diff_bnftp_pipeline.py
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


def _first_reply_len(host, port):
    import socket
    s = socket.create_connection((host, port), timeout=4.0)
    s.settimeout(3.0)
    s.sendall(b"\x02")  # BNFTP init
    s.sendall(bn.build_file_req("a.bin") + bn.build_file_req("longname.dat"))
    time.sleep(0.4)
    data = b""
    try:
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    s.close()
    r = bn.parse_reply(data) or {}
    return r.get("filelen"), len(r.get("file_data", b""))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6408)
    ap.add_argument("--v3-port", type=int, default=6508)
    args = ap.parse_args()
    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        for n, name in [(50, "a.bin"), (7, "longname.dat")]:
            content = bytes((i % 251) for i in range(n))
            with open(os.path.join(orig.home, "var", "files", name), "wb") as f:
                f.write(content)
            with open(os.path.join(v3.workdir, name), "wb") as f:
                f.write(content)
        time.sleep(0.3)
        o = _first_reply_len("127.0.0.1", args.orig_port)
        n = _first_reply_len("127.0.0.1", args.v3_port)
        print(f"first reply (filelen,bytes): oracle={o}  v3={n}")
        # Decisive observable for the use-after-erase bug: the FIRST reply names
        # a.bin (filelen 50) on both — not the second request's file (len 7) or a
        # corrupted/empty name. (The oracle additionally pipelines the second
        # file in the same stream, so its captured byte count is larger; v3 serves
        # one file per connection — a separate, documented minor difference.)
        success = (o[0] == 50 and n[0] == 50 and n[1] == 50)
        if success:
            print("BNFTP pipelined requests: first reply is correct (matches oracle).")
            return 0
        print("FAIL: BNFTP pipelined-request divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
