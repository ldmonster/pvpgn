#!/usr/bin/env python3
"""READUSERDATA with an empty key skips it (oracle), not a "" value."""
import argparse, os, sys, struct, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
def vals(host, port):
    c,_ = bc.full_login(host, port, "ek", "pw")
    bc.write_userdata(c, "ek", {"profile\\sex":"m", "profile\\age":"42"})
    keys=["profile\\sex","","profile\\age"]
    body = struct.pack("<III",1,len(keys),0)+bc.cstring("ek")
    for k in keys: body += bc.cstring(k)
    c.send(bc.SID_READUSERDATA, body); time.sleep(0.4)
    r = bc._drain_until(c, bc.SID_READUSERDATA); c.close()
    if not r: return None
    nc,kc,_ = struct.unpack_from("<III",r,0); parts=r[12:].split(b"\x00")
    return [p.decode("latin-1") for p in parts if p!=b""] or [x.decode("latin-1") for x in parts[:-1]]
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--orig-repo",default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd",default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port",type=int,default=6422); ap.add_argument("--v3-port",type=int,default=6522)
    a=ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=vals("127.0.0.1",a.orig_port); n=vals("127.0.0.1",a.v3_port)
        print(f"values: oracle={o} v3={n}")
        ok = (o==n and o==['m','42'])
        print("OK: empty key skipped, matches oracle" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
