#!/usr/bin/env python3
"""Differential test: game advertise + list (SID_STARTADVEX3 0x1C / GETADVLISTEX 0x09).

Drives, against BOTH the original pvpgn-server and v3:

    conn A (alice): login, SID_STARTADVEX3  -> advertise a public game
    conn B (bob):   login, SID_GETADVLISTEX -> the advertised game appears

and diffs the set of advertised game names each server returns. The exact
per-entry fields (port/ip/status/statstring) are not compared (v3 does not track
the host's UDP port/ip), but list membership — a hosted public game becoming
visible to other clients — is the stable, faithful observable.

Regression guard for the v3 game-advertisement wiring (on(StartGame4Request) →
StartGame; on(GameListRequest) → ListPublicGames over the shared game repo).

Run: python3 tests/diff/diff_gamelist.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

GAME = "DiffGame"


def scenario(host, port):
    out = {}
    # B first lists an empty server.
    bob, _ = bc.full_login(host, port, "bob", "bobpass")
    out["before"] = bc.game_list(bob, gametype=0x0000)

    # A advertises a public game.
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    out["adv_ack"] = bc.advertise_game(alice, GAME, info="map\r\n8")

    # B lists again — the game should now be visible.
    out["after"] = bc.game_list(bob, gametype=0x0000)

    alice.close()
    bob.close()
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
        # Compare the decisive observables: empty before, GAME present after.
        rows = [
            ("before_empty", o["before"] == [], n["before"] == []),
            ("advertised", GAME in o["after"], GAME in n["after"]),
            ("after_list", o["after"], n["after"]),
        ]
        print(f"{'field':<16}{'original':<18}{'v3':<18}match")
        print("-" * 60)
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<16}{str(ov):<18}{str(nv):<18}{'OK' if same else 'DIFF'}")
        success = (all_ok and (GAME in o["after"]) and (o["before"] == []))
        print()
        if success:
            print("Game advertise/list matches the oracle "
                  "(hosted game visible to other clients).")
            return 0
        print("FAIL: divergence in game advertise/list.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
