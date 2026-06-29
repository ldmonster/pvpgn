#!/usr/bin/env python3
"""DIFFERENTIAL D2CS GAMELISTREQ with an active game: oracle vs v3.

Completes the game-lobby: a client creates a game, the hosting D2GS reports a
player entered (UPDATEGAMEINFO ENTER -> currchar=1), then the client issues
GAMELISTREQ. d2cs must reply one GAMELISTREPLY per game with currchar>0
(seqno, token=game number, currchar, gameflag, name, desc) followed by the
end-of-list terminator (token/currchar/gameflag zero + 3 empty strings).

Both servers run the same flow through a game-hosting mock D2GS; the full
GAMELISTREPLY stream (per-game entry + terminator) must be byte-identical.
gameflag must be gameflag_create(ladder=0, expansion, hardcore, difficulty)
(0x00100004 here = release | expansion).
"""
import argparse
import os
import struct
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from d2cs_server import OriginalD2cs, V3D2cs  # noqa: E402
import bncs_client as bc  # noqa: E402
import d2cs_client as dc  # noqa: E402
import d2gs_client as dg  # noqa: E402

D2GS_GAMEID = 0x1000  # the id the mock D2GS returns in its CREATEGAMEREPLY


def creategamereq_body(seqno, name, desc):
    return (struct.pack("<HIBBB", seqno, 0x100002, 0, 0, 4)
            + name.encode() + b"\x00" + b"\x00" + desc.encode() + b"\x00")


def collect_gamelist(c, seqno):
    """Send GAMELISTREQ and collect all GAMELISTREPLY (0x05) packet bodies."""
    c.send(0x05, struct.pack("<HI", seqno, 0))
    pkts = []
    for _ in range(8):
        r = c.recv()
        if r is None:
            break
        ptype, body = r
        if ptype == 0x05:
            pkts.append(body)
    return pkts


def run(d2cs_port, login_fn):
    gs = dg.D2gsClient("127.0.0.1", d2cs_port)
    hs = gs.handshake()
    if not hs or hs["reply"] != 0:
        gs.close(); return None
    gs.send_setgsinfo(maxgame=100)
    time.sleep(0.4)
    t = threading.Thread(target=lambda: gs.serve(max_packets=3, timeout=4.0),
                         daemon=True)
    t.start()

    c, extra = login_fn()
    c.create_char("Conan", char_class=4, status=0x20)
    c.char_login("Conan")
    c.send(0x03, creategamereq_body(1, "MyGame", "A desc"))
    c.recv_type(0x03)
    t.join(timeout=5)

    # The D2GS reports a player entered -> currchar becomes 1.
    gs.send_updategameinfo(gameid=D2GS_GAMEID, charname="Conan",
                           flag=1, charlevel=1, charclass=4)
    time.sleep(0.5)

    pkts = collect_gamelist(c, 2)
    c.close()
    if extra:
        extra.close()
    gs.close()
    return pkts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--bnetd-port", type=int, default=9340)
    ap.add_argument("--orig-d2cs-port", type=int, default=9360)
    ap.add_argument("--v3-d2cs-port", type=int, default=9380)
    a = ap.parse_args()

    bnetd = OriginalBnetd(a.orig_repo, a.bnetd_port,
                          realm={"name": "test", "d2cs_port": a.orig_d2cs_port})
    od = OriginalD2cs(a.orig_repo, a.orig_d2cs_port,
                      bnetd_port=a.bnetd_port, realm_name="test")
    v3 = V3D2cs(a.v3_d2cs, a.v3_d2cs_port)
    try:
        bnetd.start(); od.start(); v3.start()
        time.sleep(1.5)

        def o_login():
            cli, _ = bc.full_login("127.0.0.1", a.bnetd_port, "gla", "pw",
                                   product=b"D2DV")
            rj = bc.realm_join(cli, "test", seqno=1)
            c = dc.D2csClient("127.0.0.1", a.orig_d2cs_port)
            c.login("gla", sessionnum=rj["sessionnum"], sessionkey=rj["sessionkey"],
                    secret_hash_raw=rj["secret_hash"], seqno=1)
            return c, cli

        def v_login():
            c = dc.D2csClient("127.0.0.1", a.v3_d2cs_port)
            c.login("gla", sessionnum=1,
                    secret_hash_raw=dc.d2cs_token("gla", 1, 7), seqno=7)
            return c, None

        o = run(a.orig_d2cs_port, o_login)
        n = run(a.v3_d2cs_port, v_login)
        print(f"oracle gamelist pkts: {[p.hex() for p in o] if o else None}")
        print(f"v3     gamelist pkts: {[p.hex() for p in n] if n else None}")
        ok = (o is not None and n is not None and len(o) >= 2 and o == n)
        print("OK: GAMELISTREPLY (per-game entry + terminator) byte-identical"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); od.stop(); bnetd.stop()


sys.exit(main())
