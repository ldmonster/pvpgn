#!/usr/bin/env python3
"""GETADVLISTEX per-entry fixed constants unknown1=1, unknown3=2, unknown6=0x2b."""
import argparse, os, sys, struct, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
def first_entry_consts(host, port):
    bob, _ = bc.full_login(host, port, "glb", "pw")
    alice, _ = bc.full_login(host, port, "gla", "pw")
    bc.advertise_game(alice, "GLGame", info="map\r\n8")
    body = struct.pack("<HHIII", 0, 0, 0, 0, 0) + bc.cstring("") + bc.cstring("")
    bob.send(bc.SID_GETADVLISTEX, body)
    time.sleep(0.4)
    reply = bc._drain_until(bob, bc.SID_GETADVLISTEX)
    alice.close(); bob.close()
    if not reply or len(reply) < 8+28: return None
    hdr = reply[8:8+28]
    _g, u1, u3 = struct.unpack_from("<HHH", hdr, 0)
    u6 = struct.unpack_from("<I", hdr, 24)[0]
    return (u1, u3, u6)
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6414); ap.add_argument("--v3-port", type=int, default=6514)
    a = ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=first_entry_consts("127.0.0.1",a.orig_port); n=first_entry_consts("127.0.0.1",a.v3_port)
        print(f"entry (u1,u3,u6): oracle={o} v3={n}")
        ok = (o==(1,2,0x2b) and n==(1,2,0x2b))
        print("OK: per-entry constants match oracle" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
