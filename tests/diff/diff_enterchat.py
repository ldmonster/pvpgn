#!/usr/bin/env python3
"""SID_ENTERCHAT with empty username: reply unique_name+account = logged-in name."""
import argparse, os, sys, struct, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
def fields(host, port):
    c = bc.BncsClient(host, port); ctok=0xDEADBEEF
    stok,_,_ = bc.auth_handshake(c, client_token=ctok)
    bc.create_account_ols(c, "alice", "pw")
    if bc.login_ols(c, "alice", "pw", ctok, stok) != 0: c.close(); return None
    c.send(bc.SID_ENTERCHAT, bc.cstring("")+bc.cstring(""))  # empty user+statstring
    time.sleep(0.3); body = bc._drain_until(c, bc.SID_ENTERCHAT); c.close()
    if body is None: return None
    parts = body.split(b"\x00")
    return (parts[0].decode("latin-1"), parts[2].decode("latin-1") if len(parts)>2 else "")  # unique_name, account
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--orig-repo",default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd",default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port",type=int,default=6424); ap.add_argument("--v3-port",type=int,default=6524)
    a=ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=fields("127.0.0.1",a.orig_port); n=fields("127.0.0.1",a.v3_port)
        print(f"(unique_name, account): oracle={o} v3={n}")
        ok = (o==n and o==("alice","alice"))
        print("OK: ENTERCHAT names the logged-in account" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
