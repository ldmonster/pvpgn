#!/usr/bin/env python3
"""Differential test: WOL game channels stay out of the plain LIST (327) section.

The original (handle_wol.cpp:576-578) explicitly skips channels whose
channel_wol_get_game_type() != 0 when emitting the RPL_CHANNEL (327) section of
LIST: game lobbies are surfaced only through the dedicated games-list path, never
mixed in with ordinary chat channels. v3 used to emit every channel from the
channel repo, so a JOINGAME-created lobby leaked into the chat-channel listing.

Scenario, driven against BOTH servers (WOL listener):
  host conn : JOINGAME #wlgame (create)  -> a game channel
  ctrl conn : JOIN #wlchat               -> a normal chat channel (control)
  obs  conn : LIST                       -> collect the 327 channel-name tokens

Diffs: whether the game channel and the chat channel appear in the 327 section.
A match means the game channel is ABSENT on both while the chat channel is
PRESENT on both.

Run: python3 tests/diff/diff_wol_list_gamechan.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def collect_list_names(client):
    """Return the lowercased 327 RPL_CHANNEL channel-name tokens."""
    client.send_line("LIST")
    names = []
    client.sock.settimeout(0.5)
    for _ in range(200):
        try:
            line = client.read_line()
        except Exception:
            break
        if line is None:
            break
        if wc.WolClient.numeric(line) == 323:  # RPL_LISTEND
            break
        if wc.WolClient.numeric(line) == 327:
            parts = line.split()
            if len(parts) >= 4:
                names.append(parts[3].lstrip("#").lower())
    return names


def scenario(host, port, sku=1000):
    host_c = wc.wol_session(host, port, "wlhost", "secretpass", sku=sku)
    chat_c = wc.wol_session(host, port, "wlctrl", "secretpass", sku=sku)
    obs = wc.wol_session(host, port, "wlobs", "secretpass", sku=sku)
    if not all((host_c, chat_c, obs)):
        for c in (host_c, chat_c, obs):
            if c:
                c.close()
        return {"game_in_list": None, "chat_in_list": None}
    try:
        wc.wol_join(chat_c, "#wlchat")
        time.sleep(0.3)
        wc.wol_joingame_create(host_c, "#wlgame", 2, 8, 1)
        time.sleep(0.5)
        names = collect_list_names(obs)
        return {
            "game_in_list": "wlgame" in names,
            "chat_in_list": "wlchat" in names,
        }
    finally:
        for c in (host_c, chat_c, obs):
            try:
                c.close()
            except Exception:
                pass


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6438)
    ap.add_argument("--v3-port", type=int, default=6538)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(0.5)
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<16}{'original':<14}{'v3':<14}match")
        print("-" * 54)
        ok = True
        for k in ("game_in_list", "chat_in_list"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<16}{ov:<14}{nv:<14}{'OK' if m else 'DIFF'}")
        print()

        if (ok and o["game_in_list"] is False
                and o["chat_in_list"] is True):
            print("WOL LIST game-channel filtering matches the oracle.")
            return 0
        print("FAIL: WOL LIST game-channel divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
