#!/usr/bin/env python3
"""CLIENT_JOIN_GAME (0x22) gets NO server reply.

The original (_client_joingame) records the join (conn_set_game) and returns
WITHOUT sending any reply packet — the joiner reaches the host peer-to-peer. v3
used to send a spurious SID_STARTADVEX3 (0x1C) StartGame4Ack on JOINGAME, an
unsolicited packet the real client never expects. Both servers must now stay
silent (answer only PINGs) after a CLIENT_JOIN_GAME.
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_JOINGAME = 0x22


def probe(host, port):
    c, _ = bc.full_login(host, port, "jgs", "pw")
    bc.drain_chat(c, 0.3)
    # CLIENT_JOIN_GAME: clienttag(u32) + versiontag(u32) + name\0 + pass\0
    body = struct.pack("<II", 0x53455850, 0) + bc.cstring("NoSuchGame") + bc.cstring("")
    c.send(SID_JOINGAME, body)
    # Collect any non-PING packets the server volunteers in response.
    import time
    time.sleep(0.6)
    c.sock.settimeout(0.4)
    extra = []
    try:
        while True:
            r = c.recv()
            if r is None:
                break
            sid, b = r
            if sid == bc.SID_PING:
                c.send(bc.SID_PING, b[:4])
                continue
            extra.append(hex(sid))
    except Exception:
        pass
    c.close()
    return extra


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6464)
    ap.add_argument("--v3-port", type=int, default=6564)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle extra packets: {o}")
        if a.orig_only:
            return 0 if o == [] else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3     extra packets: {n}")
            ok = (o == [] and n == [])
            print("OK: CLIENT_JOIN_GAME draws no reply on either server"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
