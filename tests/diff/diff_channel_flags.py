#!/usr/bin/env python3
"""EID_CHANNEL flags carry CF_PUBLIC (0x01) for a predefined permanent channel."""
import argparse, os, sys, struct, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
EID_CHANNEL=0x07
def chanflags(host, port, name):
    c,_ = bc.full_login(host, port, "cf", "pw")
    c.send(bc.SID_JOINCHANNEL, struct.pack("<I",0)+bc.cstring(name))
    time.sleep(0.4); c.sock.settimeout(0.5); flags=None
    try:
        while True:
            r=c.recv()
            if r is None: break
            if r[0]==bc.SID_PING: c.send(bc.SID_PING, r[1][:4]); continue
            if r[0]==bc.SID_CHATEVENT:
                eid=struct.unpack_from("<I",r[1],0)[0]
                if eid==EID_CHANNEL: flags=struct.unpack_from("<I",r[1],4)[0]
    except Exception: pass
    c.close(); return flags
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--orig-repo",default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd",default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port",type=int,default=6432); ap.add_argument("--v3-port",type=int,default=6532)
    a=ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=chanflags("127.0.0.1",a.orig_port,"Chat"); n=chanflags("127.0.0.1",a.v3_port,"Chat")
        print(f"EID_CHANNEL flags for 'Chat': oracle={o} v3={n}")
        ok=(o==n and (o or 0)&0x01); print("OK: CF_PUBLIC set on both" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
