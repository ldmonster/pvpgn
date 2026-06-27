#!/usr/bin/env python3
"""Differential test: BNFTP CLIENT_FILE_REQ for an empty / directory name.

A BNFTP CLIENT_FILE_REQ whose filename is empty ("") — or any name that
stat()s to a directory, e.g. "." — is accepted by the original pvpgn. In
src/bnetd/file.cpp, file_get_info only rejects rawnames containing '/' or
'\\'; an empty name passes. It builds filedir + "/" + rawname and stat()s it,
which SUCCEEDS for a directory (the files directory itself, or filedir/.). It
takes st_size as filelen and st_mtime as the timestamp, then file_send fopen()s
the directory ("rb" succeeds on Linux), sees startoffset(0) < filelen and
ALWAYS pushes the SERVER_FILE_REPLY header. The first fread() then fails
(EISDIR) so ZERO payload bytes follow — header alone, data_bytes == 0.

Regression guard for a fixed v3 bug: v3's BnftpFsm rejected the empty name in
is_safe_filename and errored on std::filesystem::file_size() for a directory,
sending NOTHING in both cases. v3 now mirrors the oracle's stat()-based path:
it sends a SERVER_FILE_REPLY header (filelen = the host directory's st_size)
with no payload.

Decisive observable (compare STRUCTURE, not the exact server-dependent
filelen): BOTH servers send a parseable SERVER_FILE_REPLY (type 0x0000) with
data_bytes == 0 and the requested name echoed. The filelen happens to equal the
host directory's st_size (4096 on ext4) on both, so we also assert they agree.

Run: python3 tests/diff/diff_bnftp_emptyname.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bnftp_client as bn  # noqa: E402

# Names that resolve to a directory (not a regular file) on both servers.
CASES = ["", "."]


def _fetch(port, name):
    c = bn.BnftpClient("127.0.0.1", port, timeout=2.5)
    c.request_file(name)
    data, _closed = c.recv_all()
    c.close()
    return data


def _check(name, o_raw, v_raw):
    """Return (ok, detail). Compares reply STRUCTURE, not server name/time."""
    o = bn.parse_reply(o_raw)
    v = bn.parse_reply(v_raw)
    if not o or o.get("type") != bn.SERVER_FILE_REPLY:
        return False, f"oracle: no SERVER_FILE_REPLY ({len(o_raw)} bytes)"
    if not v or v.get("type") != bn.SERVER_FILE_REPLY:
        return False, f"v3: no SERVER_FILE_REPLY ({len(v_raw)} bytes)"
    # Header present, zero payload, requested name echoed on both.
    checks = {
        "oracle data_bytes==0": o["data_bytes"] == 0,
        "v3 data_bytes==0": v["data_bytes"] == 0,
        "oracle echoes name": o["filename"] == name,
        "v3 echoes name": v["filename"] == name,
        # filelen is the host dir's st_size on both -> must agree structurally.
        "filelen agree": o["filelen"] == v["filelen"],
    }
    bad = [k for k, ok in checks.items() if not ok]
    if bad:
        return False, "; ".join(bad) + f" (o.filelen={o['filelen']} "\
            f"v.filelen={v['filelen']})"
    return True, f"REPLY filelen={v['filelen']} data_bytes=0"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6796)
    ap.add_argument("--v3-port", type=int, default=6896)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(0.3)

        hdr = f"{'request':>10} {'oracle':>8} {'v3':>6}  result"
        print(hdr)
        print("-" * (len(hdr) + 20))
        all_ok = True
        for name in CASES:
            o = _fetch(args.orig_port, name)
            v = _fetch(args.v3_port, name)
            ok, detail = _check(name, o, v)
            all_ok &= ok
            label = repr(name)
            print(f"{label:>10} {len(o):>8} {len(v):>6}  "
                  f"{'OK' if ok else 'DIFF'}: {detail}")
        print()
        if all_ok:
            print("BNFTP empty/dir name: SERVER_FILE_REPLY header, no payload, "
                  "matching the oracle.")
            return 0
        print("FAIL: BNFTP empty/dir-name reply divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
