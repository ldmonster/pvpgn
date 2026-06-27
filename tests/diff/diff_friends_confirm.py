#!/usr/bin/env python3
"""/friends add and /friends remove emit an EID_INFO confirmation (count)."""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
EID_INFO=0x12
def counts(host, port):
    bob, _ = bc.full_login(host, port, "fcbob", "pw"); bob.close()  # ensure exists
    alice, _ = bc.full_login(host, port, "fcalice", "pw")
    add_ev = bc.chat_command(alice, "/friends add fcbob")
    rem_ev = bc.chat_command(alice, "/friends remove fcbob")
    alice.close()
    na = sum(1 for e in add_ev if e[0]==EID_INFO)
    nr = sum(1 for e in rem_ev if e[0]==EID_INFO)
    return (na, nr)
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6416); ap.add_argument("--v3-port", type=int, default=6516)
    a = ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=counts("127.0.0.1",a.orig_port); n=counts("127.0.0.1",a.v3_port)
        print(f"EID_INFO (add,remove): oracle={o} v3={n}")
        ok = (o[0]>=1 and o[1]>=1 and n[0]==o[0] and n[1]==o[1])
        print("OK: add/remove emit confirmation like oracle" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
