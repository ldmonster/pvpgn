#!/usr/bin/env python3
"""SID_GETADVLISTEX (0x09) game record reports the game's ACTUAL type, not the filter.

The original emits glgame.gametype = gtype_to_bngtype(game_get_type(game)) — each
listed game's real type. v3 used to set e.gametype = m.gametype, echoing the
REQUESTED filter, so listing with filter=ALL(0) reported gametype=0 for every game.

v3 now stores the advertised SID_STARTADVEX3 gametype on the Game and returns it in
the GETADVLISTEX record. Advertise a game with a specific type, list with filter=ALL,
and the record's gametype must match the advertised value on both servers.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

GTYPE = 0x0002  # the gametype advertised


def probe(host, port):
    host_c, _ = bc.full_login(host, port, "glga", "pw")
    bc.advertise_game(host_c, "GtypeGame", gametype=GTYPE)
    lister, _ = bc.full_login(host, port, "glgb", "pw")
    recs = bc.game_list_detailed(lister, gametype=0x0000)  # filter = ALL
    host_c.close(); lister.close()
    rec = next((r for r in recs if r["name"] == "GtypeGame"), None)
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6460)
    ap.add_argument("--v3-port", type=int, default=6560)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle: {o}")
        if a.orig_only:
            return 0 if o else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3    : {n}")
            ok = (o is not None and n is not None
                  and o["gametype"] == n["gametype"]
                  and n["gametype"] == GTYPE)
            print(f"OK: GETADVLISTEX reports the advertised gametype 0x{GTYPE:02x} on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
