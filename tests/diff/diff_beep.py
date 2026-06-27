#!/usr/bin/env python3
"""/beep and /nobeep reply EID_INFO (not the unknown-command EID_ERROR)."""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
EID_INFO=0x12; EID_ERROR=0x13
def kind(host, port, cmd):
    c,_ = bc.full_login(host, port, "bp", "pw")
    bc.join_channel(c, "BP")
    evs = bc.chat_command(c, cmd); c.close()
    for e in evs:
        if e[0] in (EID_INFO, EID_ERROR): return "info" if e[0]==EID_INFO else "error"
    return "none"
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--orig-repo",default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd",default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port",type=int,default=6430); ap.add_argument("--v3-port",type=int,default=6530)
    a=ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        ok=True
        for cmd in ("/beep","/nobeep"):
            o=kind("127.0.0.1",a.orig_port,cmd); n=kind("127.0.0.1",a.v3_port,cmd)
            m=(o==n=="info"); ok&=m; print(f"{cmd}: oracle={o} v3={n} {'OK' if m else 'DIFF'}")
        print("OK" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
