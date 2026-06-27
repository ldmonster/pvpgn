#!/usr/bin/env python3
"""WOL COPYRIGHT/WARRANTY/LICENSE -> 15 PAGE lines; VERSION -> 1 PAGE 'PvPGN ...'."""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import wol_client as wc
def drain(c, s=0.8):
    time.sleep(s); c.sock.settimeout(0.5); out=[]
    try:
        while True:
            l=c.read_line()
            if l is None: break
            out.append(l)
    except Exception: pass
    return out
def page_lines(lines):
    return [l for l in lines if " PAGE " in l]
def scenario(host, port):
    c = wc.wol_session(host, port, "cpw", "pw")
    if c is None: return None
    c.send_line("COPYRIGHT"); cp = page_lines(drain(c))
    c.send_line("VERSION"); ver = page_lines(drain(c))
    c.close()
    has_copy = any("Copyright" in l for l in cp)
    has_ver = any("PvPGN" in l for l in ver)
    return {"copyright_pages": len(cp), "copyright_has_text": has_copy,
            "version_pages": len(ver), "version_has_pvpgn": has_ver}
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6420); ap.add_argument("--v3-port", type=int, default=6520)
    a = ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=scenario("127.0.0.1",orig.wolv1_port); n=scenario("127.0.0.1",v3.wol_port)
        if not o or not n: print("FAIL: setup"); return 1
        print(f"oracle={o}\nv3    ={n}")
        ok = (o["copyright_pages"]==n["copyright_pages"]>0 and o["copyright_has_text"] and n["copyright_has_text"]
              and o["version_pages"]==n["version_pages"]==1 and o["version_has_pvpgn"] and n["version_has_pvpgn"])
        print("OK: COPYRIGHT/VERSION match oracle" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
