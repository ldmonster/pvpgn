#!/usr/bin/env python3
"""Differential test: SID_MOTD_W3 (0x46) welcome reply.

A logged-in WarCraft III client sends CLIENT_MOTD_W3 (0x46, body = a single u32
`last_news_time`) and BLOCKS for the server's response.  The original
(_client_motdw3, handle_bnet.cpp, registered ONLY in the logged-in handler
table) answers with:

  * zero or more news entries (one SERVER_MOTD_W3 packet each, in the
    news-config order), then
  * exactly one "welcome" packet whose timestamp2 == SERVER_MOTD_W3_WELCOME (0)
    and whose text is the bnmotd_w3 file contents (empty in the default config).

Each packet body is: u8 msg_type (== 1), u32 curr_time, u32 first_news_time,
u32 timestamp, u32 timestamp2, then a NUL-terminated text string.

v3 has no news subsystem, so it sends ONLY the welcome packet — the behaviour of
an oracle with no news configured.  Before this fix v3's on(MotdRequest) was a
no-op and sent NOTHING, leaving a W3 client stalled after login.

What is compared (decisive, config-independent observables — the harness oracle
*does* carry a stock "No news today" news entry, so the news-packet count and
the per-packet timestamps are intentionally NOT compared):
  1. Both servers send at least one 0x46 packet (v3 used to send zero).
  2. Both send a welcome packet (timestamp2 == 0) with msg_type == 1.
  3. The welcome packet's curr_time is a plausible server wall clock (> 0).

Run: python3 tests/diff/diff_motd_w3.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_MOTD_W3 = 0x46


def _parse(body):
    """Return (msg_type, curr, fnews, ts, ts2, text) or None if too short."""
    if body is None or len(body) < 17:
        return None
    msg_type = body[0]
    curr, fnews, ts, ts2 = struct.unpack_from("<IIII", body, 1)
    text = body[17:]
    return (msg_type, curr, fnews, ts, ts2, text)


def scenario(host, port):
    out = {}
    c, _ = bc.full_login(host, port, "alice", "alicepass")
    c.send(SID_MOTD_W3, struct.pack("<I", 0))

    pkts = []
    for _ in range(40):
        r = c.recv()
        if r is None:
            break
        sid, body = r
        if sid == bc.SID_PING:
            c.send(bc.SID_PING, body[:4])
            continue
        if sid == SID_MOTD_W3:
            pkts.append(body)
    c.close()

    parsed = [p for p in (_parse(b) for b in pkts) if p is not None]
    welcome = next((p for p in parsed if p[4] == 0), None)
    out["any_packet"] = len(parsed) > 0
    out["has_welcome"] = welcome is not None
    out["welcome_msgtype"] = welcome[0] if welcome else None
    out["welcome_curr_pos"] = (welcome[1] > 0) if welcome else None
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=10960)
    ap.add_argument("--v3-port", type=int, default=10980)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<20}{'original':<14}{'v3':<14}match")
        print("-" * 58)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<20}{str(o[k]):<14}{str(n[k]):<14}{'OK' if same else 'DIFF'}")
        print()

        # Guard the probe itself: the oracle must send a well-formed welcome packet.
        oracle_ok = (o.get("any_packet") and o.get("has_welcome")
                     and o.get("welcome_msgtype") == 1
                     and o.get("welcome_curr_pos") is True)
        if not oracle_ok:
            print("FAIL: oracle did not behave as expected — probe/setup broken.")
            return 2
        if all_ok:
            print("SID_MOTD_W3 welcome reply matches the oracle.")
            return 0
        print("FAIL: v3 diverges from the oracle on SID_MOTD_W3.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
