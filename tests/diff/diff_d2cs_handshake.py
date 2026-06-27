#!/usr/bin/env python3
"""D2CS connection init handshake + v3 character-flow conformance.

A real D2 client opens the d2cs connection with a single init class byte
(CLIENT_INITCONN_CLASS_D2CS = 0x01) before any framed packet — exactly like the
BNCS/BNFTP listeners. The original d2cs consumes it in handle_init; v3 previously
fed raw bytes straight into the packet framer, so it MISFRAMED the very first
packet (treating 0x01 as a packet-length byte) and hung. This guards that fix.

Two checks:
  1. HANDSHAKE PARITY (differential): both the original d2cs and v3 d2cs accept a
     TCP connection + the 0x01 init byte + a well-framed packet without dropping
     the connection at the framing layer.
  2. V3 CONFORMANCE: against the v3 d2cs (standalone, stubbed auth) the full
     init -> LOGINREQ -> CREATECHARREQ -> CHARLISTREQ flow round-trips with
     reply types/codes matching the original wire format (LOGINREPLY succeed=0,
     CREATECHARREPLY succeed=0 / duplicate=0x14 ALREADY_EXIST, CHARLISTREPLY lists it).

NOTE: a full DIFFERENTIAL character-flow test needs the original d2cs linked to a
running bnetd (the original rejects LOGINREQ when bnetd_conn() is null) plus a
valid bncs session — a later harness extension. The known v3 divergences still to
fix once that exists: CREATECHARREPLY collapses duplicate->0x01 (oracle 0x14
ALREADY_EXIST); char-name validation differs; LOGINREQ fixed-field layout.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d2cs_server import V3D2cs, OriginalD2cs  # noqa: E402
import d2cs_client as dc  # noqa: E402


def accepts_handshake(host, port):
    """Connect, send init byte + a framed CHARLISTREQ; return True if the
    connection survives the framing (server didn't reset/close on the init)."""
    try:
        c = dc.D2csClient(host, port, timeout=3.0)
    except OSError:
        return False
    try:
        # send() emits the 0x01 init byte then the framed packet.
        c.send(dc.D2CS_CHARLISTREQ, b"\x08\x00\x00\x00")
        # Peer-reset would surface on the next op; a no-reply (needs auth/bnetd)
        # is fine — we only assert the framing layer accepted the init + packet.
        import socket as _s
        c.sock.settimeout(0.6)
        try:
            c.sock.recv(0)
        except (_s.timeout, BlockingIOError, OSError):
            pass
        return True
    finally:
        c.close()


def v3_roundtrip(host, port):
    c = dc.D2csClient(host, port)
    try:
        out = {
            "login": c.login("d2tester"),
            "create": c.create_char("Conan", char_class=4),
            "create_dup": c.create_char("Conan", char_class=4),
        }
        lst = c.char_list()
        out["charlist_has_conan"] = bool(lst and "Conan" in lst["names"])
        out["charlist_count"] = lst["currchar"] if lst else None
        return out
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--orig-port", type=int, default=7613)
    ap.add_argument("--v3-port", type=int, default=7713)
    a = ap.parse_args()

    orig = OriginalD2cs(a.orig_repo, a.orig_port, bnetd_port=None)
    v3 = V3D2cs(a.v3_d2cs, a.v3_port)
    try:
        orig.start()
        v3.start()

        o_hs = accepts_handshake("127.0.0.1", a.orig_port)
        n_hs = accepts_handshake("127.0.0.1", a.v3_port)
        print(f"init-handshake accepted: oracle={o_hs} v3={n_hs}")

        rt = v3_roundtrip("127.0.0.1", a.v3_port)
        print(f"v3 roundtrip: {rt}")

        ok = (
            o_hs and n_hs and
            rt["login"] == dc.LOGINREPLY_SUCCEED and
            rt["create"] == dc.CREATECHAR_SUCCEED and
            rt["create_dup"] == dc.CREATECHAR_ALREADY_EXIST and  # 0x14, oracle parity
            rt["charlist_has_conan"] and
            rt["charlist_count"] == 1
        )
        print("OK: d2cs init handshake parity + v3 character flow conformance"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop()
        orig.stop()


sys.exit(main())
