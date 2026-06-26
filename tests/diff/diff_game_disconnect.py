#!/usr/bin/env python3
"""Differential test: advertised game is removed when its host DISCONNECTS.

Lifecycle bug-hunt (analogous to the WOL game-ghost fix, wave 51, but for the
BNCS GETADVLISTEX surface). Drives, against BOTH the original pvpgn-server and
v3:

    conn A (host):  login, SID_STARTADVEX3  -> advertise a public game
    conn B (lister): login, SID_GETADVLISTEX -> the game appears (sanity)
    conn A: RAW DISCONNECT (no SID_CLOSEGAME)
    conn B: SID_GETADVLISTEX -> the game must be GONE, not a ghost

The original removes a connection's game on connection teardown. If v3's
on_disconnect() only leaves the channel (and not the game), the advertised game
ghosts in GETADVLISTEX forever — exactly the divergence this checks.

Run: python3 tests/diff/diff_game_disconnect.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

GAME = "GhostGame"


def scenario(host, port):
    out = {}
    lister, _ = bc.full_login(host, port, "bob", "bobpass")

    host_c, _ = bc.full_login(host, port, "alice", "alicepass")
    out["adv_ack"] = bc.advertise_game(host_c, GAME, info="map\r\n8")
    out["while_hosted"] = bc.game_list(lister, gametype=0x0000)

    # Host vanishes WITHOUT a clean SID_CLOSEGAME — a raw socket close.
    host_c.close()
    time.sleep(0.8)  # let each server process the disconnect

    out["after_disconnect"] = bc.game_list(lister, gametype=0x0000)

    lister.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6374)
    ap.add_argument("--v3-port", type=int, default=6474)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        rows = [
            ("hosted_visible", GAME in o["while_hosted"], GAME in n["while_hosted"]),
            ("gone_after_dc", GAME not in o["after_disconnect"],
             GAME not in n["after_disconnect"]),
            ("after_list", o["after_disconnect"], n["after_disconnect"]),
        ]
        print(f"{'field':<16}{'original':<18}{'v3':<18}match")
        print("-" * 60)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<16}{str(ov):<18}{str(nv):<18}{'OK' if same else 'DIFF'}")
        success = (all_ok
                   and (GAME in o["while_hosted"])
                   and (GAME not in o["after_disconnect"]))
        print()
        if success:
            print("Advertised game is removed on host disconnect (matches oracle).")
            return 0
        print("FAIL: host-disconnect game-ghost divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
