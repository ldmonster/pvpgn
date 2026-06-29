#!/usr/bin/env python3
"""v3-conformance: D2CS join-game cross-session routing (client + mock D2GS).

Verifies the v3 d2cs JOINGAMEREQ routing built alongside create-game: a client
creates a game (which the registry records name->host D2GS), then JOINs it by
name; d2cs looks the game up, forwards D2CS_D2GS_JOINGAMEREQ (0x21) to its host,
and on the D2GS reply answers the client JOINGAMEREPLY (0x04) with SUCCEED.

This is v3-conformance (not differential): the ORIGINAL d2cs gates joining behind
extra game state (the creator is already in its game, game open/full checks),
so a clean two-message create+join against the oracle on one connection is not
reliably reproducible. The create-game flow itself IS differentially verified in
diff_d2cs_creategame.py; here we confirm v3 routes the join symmetrically.
"""
import argparse
import os
import struct
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d2cs_server import V3D2cs  # noqa: E402
import d2cs_client as dc  # noqa: E402
import d2gs_client as dg  # noqa: E402

JOINGAME_SUCCEED = 0x00


def creategamereq_body(seqno, name):
    return (struct.pack("<HIBBB", seqno, 0x100002, 0, 0, 4)
            + name.encode() + b"\x00" + b"\x00" + b"\x00")


def joingamereq_body(seqno, name):
    return struct.pack("<H", seqno) + name.encode() + b"\x00" + b"\x00"


def parse_joingamereply(body):
    # seqno(u16) gameid(u16) u1(u16) addr(u32) token(u32) reply(u32) = 18 bytes
    if body is None or len(body) < 18:
        return None
    seqno, gameid, u1 = struct.unpack_from("<HHH", body, 0)
    addr, token, reply = struct.unpack_from("<III", body, 6)
    return {"seqno": seqno, "gameid": gameid, "addr": addr,
            "token": token, "reply": reply}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--v3-port", type=int, default=8960)
    a = ap.parse_args()

    with V3D2cs(a.v3_d2cs, a.v3_port):
        time.sleep(0.4)
        gs = dg.D2gsClient("127.0.0.1", a.v3_port)
        hs = gs.handshake()
        assert hs and hs["reply"] == 0, f"d2gs handshake failed: {hs}"
        gs.send_setgsinfo(maxgame=10)
        time.sleep(0.4)

        seen = []
        t = threading.Thread(
            target=lambda: seen.extend(gs.serve(max_packets=8, timeout=5.0)),
            daemon=True)
        t.start()

        c = dc.D2csClient("127.0.0.1", a.v3_port)
        c.login("jgacct", sessionnum=1,
                secret_hash_raw=dc.d2cs_token("jgacct", 1, 7), seqno=7)
        c.create_char("JGamer", char_class=4, status=0x20)
        c.char_login("JGamer")

        c.send(0x03, creategamereq_body(1, "JGame"))
        create_reply = c.recv_type(0x03)

        c.send(0x04, joingamereq_body(2, "JGame"))
        join = parse_joingamereply(c.recv_type(0x04))
        c.close()
        t.join(timeout=6)
        gs.close()

    print(f"create reply present: {create_reply is not None}")
    print(f"join reply: {join}")
    print(f"d2gs saw: {[hex(p) for p, _ in seen]}")

    ok = (create_reply is not None and join is not None and
          join["reply"] == JOINGAME_SUCCEED and join["seqno"] == 2 and
          0x21 in [p for p, _ in seen])
    print("OK: v3 routes JOINGAMEREQ to the host D2GS (0x21) and replies "
          "JOINGAMEREPLY SUCCEED" if ok else "FAIL")
    return 0 if ok else 1


sys.exit(main())
