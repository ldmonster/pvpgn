#!/usr/bin/env python3
"""SID_CREATE_ACCT1: a >32-char username is dropped with NO reply (oracle parity)."""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
def replied(host, port, namelen):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    name = "u" * namelen
    got = bc.create_account_ols(c, name, "pw")  # returns reply code or None on no-reply
    c.close()
    return got
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6412); ap.add_argument("--v3-port", type=int, default=6512)
    a = ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=replied("127.0.0.1",a.orig_port,40); n=replied("127.0.0.1",a.v3_port,40)
        print(f"create 40-char name reply: oracle={o!r} v3={n!r}")
        ok = (o is None and n is None)
        print("OK: both send no reply (dropped)" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
