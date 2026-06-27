#!/usr/bin/env python3
"""SID_FRIENDINFO (0x66): no reply for an empty list or out-of-range index."""
import argparse, os, sys, struct, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
SID_FRIENDINFO = 0x66
def probe(host, port):
    c, _ = bc.full_login(host, port, "fi", "pw")
    c.send(SID_FRIENDINFO, struct.pack("<B", 0))   # index 0, zero friends
    time.sleep(0.4); c.sock.settimeout(0.5); n = 0
    try:
        while True:
            r = c.recv()
            if r is None: break
            if r[0] == SID_FRIENDINFO: n += 1
            elif r[0] == bc.SID_PING: c.send(bc.SID_PING, r[1][:4])
    except Exception: pass
    c.close(); return n
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6410); ap.add_argument("--v3-port", type=int, default=6510)
    a = ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=probe("127.0.0.1",a.orig_port); n=probe("127.0.0.1",a.v3_port)
        print(f"FRIENDINFO replies (zero friends): oracle={o} v3={n}")
        ok = (o==0 and n==0)
        print("OK: no reply, matches oracle" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
