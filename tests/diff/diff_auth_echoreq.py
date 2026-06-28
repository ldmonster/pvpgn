#!/usr/bin/env python3
"""Server sends SERVER_ECHOREQ (0x25 PING) before the SID_AUTH_INFO (0x50) reply.

The original handle_bnet.cpp answers AUTH_INFO by first queuing a SERVER_ECHOREQ
(0x25) latency cookie, then SERVER_AUTHREQ_109 (0x50). v3 previously sent only the
0x50 reply. Both must now emit 0x25 before 0x50.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_PING = 0x25
SID_AUTH_INFO = 0x50


def order(host, port):
    c = bc.BncsClient(host, port)
    c.send_auth_info()
    seq = []
    for _ in range(6):
        r = c.recv()
        if r is None:
            break
        seq.append(r[0])
        if r[0] == SID_AUTH_INFO:
            break
    c.close()
    # ping seen, and it precedes the auth-info reply
    return {
        "ping_before_auth": (SID_PING in seq and SID_AUTH_INFO in seq
                             and seq.index(SID_PING) < seq.index(SID_AUTH_INFO)),
        "seq": [hex(x) for x in seq],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6432)
    ap.add_argument("--v3-port", type=int, default=6532)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = order("127.0.0.1", a.orig_port)
        n = order("127.0.0.1", a.v3_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")
        ok = o["ping_before_auth"] and n["ping_before_auth"]
        print("OK: both send 0x25 ECHOREQ before the 0x50 AUTH_INFO reply"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
