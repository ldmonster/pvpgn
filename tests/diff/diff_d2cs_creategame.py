#!/usr/bin/env python3
"""ORACLE SPEC CAPTURE: D2CS game-create routing (client + mock D2GS host).

This validates the full d2cs game-lobby create flow against the ORACLE and
captures the exact wire sequence the v3 d2cs game-routing subsystem must match:

  1. A mock D2GS connects (init 0x64), completes the AUTHREQ/AUTHREPLY
     handshake, and sends SETGSINFO (maxgame > 0) so d2cs marks it choosable.
  2. A client logs in (BNCS -> realm-join -> d2cs), creates a character, and
     selects it (char_authed).
  3. The client sends CREATEGAMEREQ (0x03). d2cs picks the D2GS via
     d2gslist_choose_server, forwards D2CS_D2GS_CREATEGAMEREQ (0x20) to it; the
     D2GS replies CREATEGAMEREPLY (0x20) SUCCEED; d2cs replies the client
     CREATEGAMEREPLY (0x03) with reply=SUCCEED and an assigned gameid.

The v3 side does NOT yet route (handle_create_game is a stub) — building the
cross-session routing (a shared d2gs-link registry + game store + seqno-
correlated async forward) is the next focused feature. This test asserts the
ORACLE spec (so the mock D2GS hosting stays verified) and prints v3's current
behaviour for visibility.
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

CREATEGAME_SUCCEED = 0x00


def creategamereq_body(seqno=1, gameflag=0x100002, maxchar=4,
                       name="MyGame", gamepass="", desc=""):
    # u16 seqno, u32 gameflag, u8 u1, u8 leveldiff, u8 maxchar, 3 cstrings.
    return (struct.pack("<HIBBB", seqno, gameflag, 0, 0, maxchar)
            + name.encode() + b"\x00"
            + gamepass.encode() + b"\x00"
            + desc.encode() + b"\x00")


def parse_creategamereply(body):
    # seqno(u16) gameid(u16) u1(u16) reply(u32)
    if body is None or len(body) < 10:
        return None
    seqno, gameid, u1, reply = struct.unpack_from("<HHHI", body, 0)
    return {"seqno": seqno, "gameid": gameid, "u1": u1, "reply": reply}


def run_oracle(bnetd_port, d2cs_port):
    gs = dg.D2gsClient("127.0.0.1", d2cs_port)
    hs = gs.handshake()
    if not hs or hs.get("reply") != 0:
        gs.close(); return None
    gs.send_setgsinfo(maxgame=10)
    time.sleep(0.3)

    seen = []
    t = threading.Thread(
        target=lambda: seen.extend(gs.serve(max_packets=4, timeout=4.0)),
        daemon=True)
    t.start()

    cli, _ = bc.full_login("127.0.0.1", bnetd_port, "cgacct", "pw", product=b"D2DV")
    rj = bc.realm_join(cli, "test", seqno=1)
    c = dc.D2csClient("127.0.0.1", d2cs_port)
    c.login("cgacct", sessionnum=rj["sessionnum"], sessionkey=rj["sessionkey"],
            secret_hash_raw=rj["secret_hash"], seqno=1)
    c.create_char("CGamer", char_class=4, status=0x20)
    c.char_login("CGamer")
    c.send(0x03, creategamereq_body())
    reply = parse_creategamereply(c.recv_type(0x03))
    c.close(); cli.close()
    t.join(timeout=5)
    gs.close()
    return {"reply": reply, "d2gs_saw": [p for p, _ in seen]}


def probe_v3(d2cs_port):
    """Report whether v3 routes a CREATEGAMEREQ yet (currently a no-op stub)."""
    try:
        gs = dg.D2gsClient("127.0.0.1", d2cs_port)
        if not gs.handshake():
            gs.close(); return "no-d2gs-link"
        gs.send_setgsinfo(maxgame=10)
        c = dc.D2csClient("127.0.0.1", d2cs_port)
        c.login("cgacct", sessionnum=1,
                secret_hash_raw=dc.d2cs_token("cgacct", 1, 7), seqno=7)
        c.create_char("CGamer", char_class=4, status=0x20)
        c.char_login("CGamer")
        c.send(0x03, creategamereq_body())
        rep = parse_creategamereply(c.recv_type(0x03))
        c.close(); gs.close()
        return rep if rep else "no-reply (stub)"
    except Exception as e:  # noqa: BLE001
        return f"error: {e}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--bnetd-port", type=int, default=8840)
    ap.add_argument("--orig-d2cs-port", type=int, default=8860)
    ap.add_argument("--v3-d2cs-port", type=int, default=8880)
    a = ap.parse_args()

    bnetd = OriginalBnetd(a.orig_repo, a.bnetd_port,
                          realm={"name": "test", "d2cs_port": a.orig_d2cs_port})
    od = OriginalD2cs(a.orig_repo, a.orig_d2cs_port,
                      bnetd_port=a.bnetd_port, realm_name="test")
    v3 = V3D2cs(a.v3_d2cs, a.v3_d2cs_port)
    try:
        bnetd.start(); od.start(); v3.start()
        time.sleep(1.5)
        o = run_oracle(a.bnetd_port, a.orig_d2cs_port)
        print(f"oracle: {o}")
        print(f"v3 (current, routing not yet implemented): {probe_v3(a.v3_d2cs_port)}")

        ok = (o is not None and o["reply"] is not None and
              o["reply"]["reply"] == CREATEGAME_SUCCEED and
              0x20 in o["d2gs_saw"])
        print("OK: oracle routes CREATEGAMEREQ to the D2GS (0x20) and replies "
              "CREATEGAMEREPLY SUCCEED — spec captured for the v3 routing subsystem"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); od.stop(); bnetd.stop()


sys.exit(main())
