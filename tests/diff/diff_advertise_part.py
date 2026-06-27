#!/usr/bin/env python3
"""Differential test: advertising a game (SID_STARTADVEX3) parts the chat channel.

The original (`_client_startgame4`, handle_bnet.cpp) unconditionally parts the
host from its current channel before advertising the game:

    // Quick hack to make W3 part channels when creating a game
    if (conn_get_channel(c))
        conn_part_channel(c);

so the remaining channel members get EID_LEAVE and the host stops ghosting in the
channel roster while it hosts. v3's BnetFsm::on(StartGame4Request) created the
game but kept the host in the channel — a ghost member the oracle would have
removed. This guards the part-on-advertise behavior.

Scenario (against BOTH servers):

    A,B: login, enter chat, JOIN #chan
    B: drains A's join notice
    A: SID_STARTADVEX3 (advertise a game)
    B: must see EID_LEAVE for A (A parted the channel on advertise)

Run: python3 tests/diff/diff_advertise_part.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "AdvPart"
A = "advpartA"
B = "advpartB"
GAME = "AdvPartGame"
EID_LEAVE = 3


def login(host, port, user, create=False):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    if create:
        bc.create_account_ols(c, user, "pw")
    rc = bc.login_ols(c, user, "pw", ctok, stok)
    if rc != 0:
        c.close()
        raise RuntimeError(f"login {user} rc={rc}")
    bc.enter_chat(c, user)
    return c


def scenario(host, port):
    b = login(host, port, B, create=True)
    bc.join_channel(b, CHAN)
    a = login(host, port, A, create=True)
    bc.join_channel(a, CHAN)
    bc.drain_chat(b)  # clear B's join notice for A

    ack = bc.advertise_game(a, GAME, info="map\r\n8")
    time.sleep(0.8)
    evs = bc.drain_chat(b)
    saw_leave = any(e[0] == EID_LEAVE and A.lower() in (e[1] or "").lower()
                    for e in evs)

    for c in (a, b):
        try:
            c.close()
        except Exception:
            pass
    return {"adv_ack": ack, "b_saw_A_leave": saw_leave}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6386)
    ap.add_argument("--v3-port", type=int, default=6486)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"{'field':<16}{'original':<12}{'v3':<12}match")
        print("-" * 52)
        all_ok = True
        for k in ("adv_ack", "b_saw_A_leave"):
            ov, nv = str(o[k]), str(n[k])
            same = ov == nv
            all_ok &= same
            print(f"{k:<16}{ov:<12}{nv:<12}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must demonstrate the part (b_saw_A_leave True) for the test
        # to be meaningful, and v3 must match it.
        if all_ok and o["b_saw_A_leave"]:
            print("Advertise-game parts the chat channel (matches oracle).")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n} "
              "(v3 likely keeps the host as a channel ghost while hosting).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
