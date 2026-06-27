#!/usr/bin/env python3
"""WOL GAMEOPT #chan while not in a channel -> 403 ERR_NOSUCHCHANNEL (not silent)."""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import wol_client as wc
def has403(host, port):
    c = wc.wol_session(host, port, "go", "pw")
    if c is None: return None
    c.send_line("GAMEOPT #ghost :speed=6")
    c.send_line("PING x"); time.sleep(0.5); c.sock.settimeout(0.5); got=False
    try:
        while True:
            l=c.read_line()
            if l is None: break
            if len(l.split())>1 and l.split()[1]=="403": got=True
    except Exception: pass
    c.close(); return got
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--orig-repo",default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd",default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port",type=int,default=6428); ap.add_argument("--v3-port",type=int,default=6528)
    a=ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=has403("127.0.0.1",orig.wolv1_port); n=has403("127.0.0.1",v3.wol_port)
        print(f"403 sent: oracle={o} v3={n}")
        ok=(o and n); print("OK" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
