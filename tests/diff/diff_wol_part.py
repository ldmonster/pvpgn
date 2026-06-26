#!/usr/bin/env python3
"""Differential test: WOL PART (channel leave) faithfulness.

The original's _handle_part_command (src/bnetd/handle_wol.cpp) IGNORES its
parameters and calls conn_part_channel(conn), which parts the connection's
CURRENT channel via channel_del_connection(message_type_part):

  * PART while NOT on a channel  -> silent no-op (NO numeric is sent).
  * PART <anything>              -> the connection leaves whatever channel it is
                                    actually on; the PART line names the REAL
                                    current channel, not the supplied parameter.
  * The PART is broadcast to the whole channel (remaining members + the leaver),
    so other members see the departure and the leaver no longer ghosts.

v3 previously: replied 442 when not on a channel, echoed the *parameter* channel
instead of the real one, only echoed to the parting user (never left the domain
channel), so other members never saw the PART. This verifies the corrected
behavior against the oracle.

The PART line's source hostmask is environment-dependent (oracle WCHT@<ip> vs v3
@Battle.net), so — like diff_wol_kick / diff_wol_names — this compares the
decisive observables (presence/absence + the channel named), not the literal
prefix.

Run: python3 tests/diff/diff_wol_part.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.5):
    time.sleep(settle)
    c.sock.settimeout(0.4)
    out = []
    try:
        while True:
            line = c.read_line()
            if line is None:
                break
            out.append(line)
    except OSError:
        pass
    return out


def _has_part_for(lines, nick, channel):
    """True iff some line is a PART naming `nick` (as source) and `channel`."""
    for ln in lines:
        if " PART " not in ln:
            continue
        src = ln[1:].split("!", 1)[0].split(" ", 1)[0] if ln.startswith(":") else ""
        after = ln.split(" PART ", 1)[1].strip()
        chan = after.split(" ", 1)[0].lstrip(":")
        if src.lower() == nick.lower() and chan.lstrip("#").lower() == channel.lstrip("#").lower():
            return True
    return False


def _any_numeric(lines):
    return any(wc.WolClient.numeric(ln) is not None for ln in lines)


def scenario(host, port):
    # --- Case 1: PART while not on any channel -> silent no-op (no numeric). ---
    a = wc.wol_session(host, port, "alfa", "pw")
    if a is None:
        return None
    _drain(a, 0.3)
    a.send_line("PART #nowhere")
    not_on_chan = _drain(a, 0.4)
    no_reply_when_off = not _any_numeric(not_on_chan) and not_on_chan == []

    # --- Case 2: PART <wrong> while in #room -> echoes the REAL channel. ---
    a.send_line("JOIN #room")
    _drain(a, 0.5)
    a.send_line("PART #wrongname")
    after = _drain(a, 0.5)
    echoes_real = _has_part_for(after, "alfa", "#room")
    echoes_param = _has_part_for(after, "alfa", "#wrongname")

    # --- Case 3: multi-user broadcast (B must see A's PART). ---
    a.send_line("JOIN #hall")
    _drain(a, 0.4)
    b = wc.wol_session(host, port, "bravo", "pw")
    if b is None:
        a.close()
        return None
    _drain(b, 0.3)
    b.send_line("JOIN #hall")
    _drain(a, 0.4)
    _drain(b, 0.4)
    a.send_line("PART #hall")
    a_self = _drain(a, 0.4)
    b_saw = _drain(b, 0.4)
    a.close()
    b.close()
    return {
        "no_reply_when_off_channel": no_reply_when_off,
        "echoes_real_channel": echoes_real,
        "ignores_param_channel": not echoes_param,
        "self_echo": _has_part_for(a_self, "alfa", "#hall"),
        "member_sees_part": _has_part_for(b_saw, "alfa", "#hall"),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6360)
    ap.add_argument("--v3-port", type=int, default=6460)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        if not o or not n:
            print("FAIL: login/setup failed")
            return 1
        fields = ["no_reply_when_off_channel", "echoes_real_channel",
                  "ignores_param_channel", "self_echo", "member_sees_part"]
        print(f"{'field':<28}{'oracle':<10}{'v3':<10}match")
        print("-" * 56)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<28}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must actually exhibit the faithful behavior, and v3 must match.
        expected = all(o[f] for f in fields)
        if all_ok and expected:
            print("WOL PART matches the oracle (no-op off-channel, real-channel "
                  "echo, broadcast to members).")
            return 0
        print("FAIL: WOL PART divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
