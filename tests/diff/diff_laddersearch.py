#!/usr/bin/env python3
"""SID_LADDERSEARCH (0x2f): the server always replies with the player's rank.

The original _client_laddersearchreq ALWAYS sends a SERVER_LADDERSEARCHREPLY
(0-based rank, or RANK_NONE 0xffffffff when the player is unranked). v3 had a
no-op handler that sent nothing, hanging the client. Neither server has ladder
data for a freshly-created account, so both must reply RANK_NONE.
"""
import argparse, os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_LADDERSEARCH = 0x2F
RANK_NONE = 0xFFFFFFFF


def rank_for(host, port):
    c, _ = bc.full_login(host, port, "ladderp", "pw")
    # clienttag 'SEXP', id=1 (standard), type=0 (highest rated), player name
    body = struct.pack("<4s2I", b"SEXP", 1, 0) + bc.cstring("ladderp")
    c.send(SID_LADDERSEARCH, body)
    rep = bc._drain_until(c, SID_LADDERSEARCH)
    c.close()
    if rep is None or len(rep) < 4:
        return None
    return struct.unpack_from("<I", rep, 0)[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6418)
    ap.add_argument("--v3-port", type=int, default=6518)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = rank_for("127.0.0.1", a.orig_port)
        n = rank_for("127.0.0.1", a.v3_port)
        print(f"LADDERSEARCH rank: oracle={o!r} v3={n!r} (expect {RANK_NONE:#x})")
        ok = (o == RANK_NONE and n == RANK_NONE)
        print("OK: both reply RANK_NONE for an unranked player" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
