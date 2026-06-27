#!/usr/bin/env python3
"""Differential test: SID_STARTGAME1 (0x08) / SID_STARTGAME3 (0x1A) status gating.

The original (_client_startgame1 / _client_startgame3, handle_bnet.cpp) only emits
a SERVER_STARTGAME{1,3}_ACK when the connection has NO current game AND the masked
status is not CLIENT_STARTGAME{1,3}_STATUS_DONE (0x0c). A status=DONE request on a
connection with no game is logged ("client tried to set game status DONE to
destroyed game") and gets NO reply; a status=OPEN request creates a game and ACKs.

This guards the fix that made v3 stop sending a spurious ACK for status=DONE:
  - status=OPEN (0x04): both servers send exactly one ACK (0x08 / 0x1A).
  - status=DONE (0x0c): both servers send ZERO reply bytes.

Run: python3 tests/diff/diff_startgame_done.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
from bncs_client import full_login  # noqa: E402

SID_STARTGAME1 = 0x08
SID_STARTGAME3 = 0x1A
STATUS_OPEN = 0x04
STATUS_DONE = 0x0C


def sg1_body(status):
    b = struct.pack("<I", status)   # status
    b += struct.pack("<I", 0)       # unknown3
    b += struct.pack("<H", 0)       # gametype
    b += struct.pack("<H", 0)       # unknown1
    b += struct.pack("<I", 0)       # unknown4
    b += struct.pack("<I", 0)       # unknown5
    b += b"mygame\x00\x00info\x00"  # gamename / pass / info
    return b


def sg3_body(status):
    b = struct.pack("<I", status)   # status
    b += struct.pack("<I", 0)       # unknown3
    b += struct.pack("<H", 0)       # gametype
    b += struct.pack("<H", 0)       # unknown1
    b += struct.pack("<I", 0)       # unknown6
    b += struct.pack("<I", 0)       # unknown4
    b += struct.pack("<I", 0)       # unknown5
    b += b"mygame\x00\x00info\x00"
    return b


def drain(c, t=0.6):
    c.sock.settimeout(t)
    out = []
    while True:
        r = c.recv()
        if r is None:
            break
        out.append((r[0], r[1].hex()))
    return out


def scenario(host, port, tag):
    res = {}
    cases = [
        ("sg1_open", SID_STARTGAME1, sg1_body(STATUS_OPEN)),
        ("sg1_done", SID_STARTGAME1, sg1_body(STATUS_DONE)),
        ("sg3_open", SID_STARTGAME3, sg3_body(STATUS_OPEN)),
        ("sg3_done", SID_STARTGAME3, sg3_body(STATUS_DONE)),
    ]
    for i, (name, sid, body) in enumerate(cases):
        c, _ = full_login(host, port, f"{tag}{i}", "pw")
        drain(c, 0.3)
        c.send(sid, body)
        res[name] = drain(c)
        c.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11640)
    ap.add_argument("--v3-port", type=int, default=11646)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port, "sgo")
        n = scenario("127.0.0.1", args.v3_port, "sgv")

        keys = ["sg1_open", "sg1_done", "sg3_open", "sg3_done"]
        print(f"{'case':<12}{'oracle':<22}{'v3':<22}match")
        print("-" * 64)
        all_ok = True
        for k in keys:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<12}{str(o[k]):<22}{str(n[k]):<22}{'OK' if same else 'DIFF'}")

        # Decisive observables: DONE -> empty on both; OPEN -> one ACK on both.
        done_silent = o["sg1_done"] == [] and n["sg1_done"] == [] \
            and o["sg3_done"] == [] and n["sg3_done"] == []
        open_acks = len(o["sg1_open"]) == 1 and len(n["sg1_open"]) == 1 \
            and len(o["sg3_open"]) == 1 and len(n["sg3_open"]) == 1
        print()
        if all_ok and done_silent and open_acks:
            print("STARTGAME1/3 status gating matches the oracle "
                  "(DONE silent, OPEN acked).")
            return 0
        print("FAIL: divergence in STARTGAME1/3 status gating.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
