#!/usr/bin/env python3
"""On disconnect, the channel EID_LEAVE (0x03) precedes the friend "has left"
EID_WHISPER (0x04) for an observer who is both a channel-mate and mutual friend.
Mirrors the oracle's conn_destroy order (channel_del_connection -> ET_logout)."""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc
EID_LEAVE=0x03; EID_WHISPER=0x04
def order(host, port):
    a0,_ = bc.full_login(host,port,"poa","pw"); a0.close(); time.sleep(0.3)
    b0,_ = bc.full_login(host,port,"pob","pw"); bc.friends_add(b0,"poa"); bc.drain_chat(b0,0.4); b0.close(); time.sleep(0.3)
    alice,_ = bc.full_login(host,port,"poa","pw"); bc.friends_add(alice,"pob"); bc.join_channel(alice,"PO"); bc.drain_chat(alice,0.4)
    bob,_ = bc.full_login(host,port,"pob","pw"); bc.join_channel(bob,"PO"); bc.drain_chat(alice,0.4)
    bob.close(); time.sleep(0.7)
    evs = bc.drain_chat(alice,0.6)
    alice.close()
    seq = [e[0] for e in evs if e[0] in (EID_LEAVE,EID_WHISPER)]
    return seq
def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),"../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6418); ap.add_argument("--v3-port", type=int, default=6518)
    a = ap.parse_args(); orig=OriginalBnetd(a.orig_repo,a.orig_port); v3=V3Bnetd(a.v3_bnetd,a.v3_port)
    try:
        orig.start(); v3.start()
        o=order("127.0.0.1",a.orig_port); n=order("127.0.0.1",a.v3_port)
        print(f"event order oracle={['%#x'%x for x in o]} v3={['%#x'%x for x in n]}")
        def leave_first(s): return EID_LEAVE in s and EID_WHISPER in s and s.index(EID_LEAVE) < s.index(EID_WHISPER)
        ok = leave_first(o) and leave_first(n)
        print("OK: LEAVE precedes WHISPER on both" if ok else "FAIL"); return 0 if ok else 1
    finally: v3.stop(); orig.stop()
sys.exit(main())
