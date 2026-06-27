#!/usr/bin/env python3
"""WOL JOINGAME error replies 478/471/475 use IRC middle-param form (no double colon)."""
import argparse, os, sys, time, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import wol_client as wc
def drain(c,s=0.6):
    time.sleep(s); c.sock.settimeout(0.5); out=[]
    try:
        while True:
            l=c.read_line()
            if l is None: break
            out.append(l)
    except Exception: pass
    return out
def norm(line):  # strip the ":<server> " prefix so only the numeric+args remain
    return re.sub(r'^:\S+ ', '', line) if line else line
def first(lines, code):
    for l in lines:
        p=l.split()
        if len(p)>1 and p[1]==code: return norm(l)
    return None
def scenario(host, port):
    res={}
    c=wc.wol_session(host,port,"jeu","pw")
    c.send_line("JOINGAME #nope 1"); res["478"]=first(drain(c),"478")
    h=wc.wol_session(host,port,"jeh","pw"); h.send_line("JOINGAME #fullg 1 1 1 1 1 0"); drain(h)
    c.send_line("JOINGAME #fullg 1"); res["471"]=first(drain(c),"471")
    ph=wc.wol_session(host,port,"jeph","pw"); ph.send_line("JOINGAME #pwg 2 2 1 1 1 0 0 secret"); drain(ph)
    c.send_line("JOINGAME #pwg 1 wrong"); res["475"]=first(drain(c),"475")
    for x in (c,h,ph): x.close()
    return res
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--orig-repo",default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd",default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port",type=int,default=6426); ap.add_argument("--v3-port",type=int,default=6526)
    a=ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=scenario("127.0.0.1",orig.wolv1_port); n=scenario("127.0.0.1",v3.wol_port)
        ok=True
        for code in ("478","471","475"):
            m = (o[code]==n[code] and o[code] is not None)
            ok &= m
            print(f"{code}: oracle={o[code]!r} v3={n[code]!r} {'OK' if m else 'DIFF'}")
        print("OK" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
