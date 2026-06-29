#!/usr/bin/env python3
"""DIFFERENTIAL D2CS<->D2GS server-link handshake: oracle vs v3.

A game server (D2GS) opens a server-to-server link to d2cs with the init class
byte CLIENT_INITCONN_CLASS_D2GS (0x64). d2cs immediately sends AUTHREQ (0x10,
carrying a sessionnum + the realm name); the D2GS answers AUTHREPLY (0x11,
version/checksum/signature); d2cs validates (version/checksum checks disabled by
default) and replies AUTHREPLY (0x11) with a result code. Both servers must
accept the link and reply SUCCEED (0).

The realm name and sessionnum are environment-specific (config / per-connection)
so they are not byte-compared; the protocol FLOW and the SUCCEED result are.

This exercises the init-class-0x64 path that v3 d2cs previously rejected
outright ("bad init class byte") — the foundation of the D2GS link.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d2cs_server import OriginalD2cs, V3D2cs  # noqa: E402
import d2gs_client as dg  # noqa: E402

SUCCEED = 0x00


def run_link(port):
    c = dg.D2gsClient("127.0.0.1", port)
    try:
        return c.handshake()
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--orig-port", type=int, default=8560)
    ap.add_argument("--v3-port", type=int, default=8580)
    a = ap.parse_args()

    # The d2gs link handshake needs no bnetd link (only client auth does).
    orig = OriginalD2cs(a.orig_repo, a.orig_port, bnetd_port=None, realm_name="test")
    v3 = V3D2cs(a.v3_d2cs, a.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(1.0)

        o = run_link(a.orig_port)
        n = run_link(a.v3_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")

        def ok_side(r):
            return (r is not None and r.get("authreq") is not None and
                    isinstance(r["authreq"].get("realmname"), str) and
                    r.get("reply") == SUCCEED)

        ok = ok_side(o) and ok_side(n)
        print(f"oracle d2gs link SUCCEED: {ok_side(o)}")
        print(f"v3     d2gs link SUCCEED: {ok_side(n)}")
        print("OK: both d2cs servers accept the D2GS link (0x64) and AUTHREPLY SUCCEED"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop()
        orig.stop()


sys.exit(main())
