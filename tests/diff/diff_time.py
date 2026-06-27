#!/usr/bin/env python3
"""Differential test: the /time chat command.

Drives, against BOTH the original pvpgn-server and v3:

    alice logs in, joins a channel, sends "/time"

Observable compared: the STRUCTURE of the reply — the number of EID_INFO (0x12)
chat events the server emits. The original's _handle_time_command
(bnetd/command.cpp) sends TWO EID_INFO lines for a Battle.net-class connection:

    "Server Time: <Wed Jun 23 15:15:29>"
    "Your local time: <...>"        (only when conn_get_class == conn_class_bnet)

A BNCS client is always bnet-class, so the oracle always emits both lines. v3
previously had no /time handler, so the command fell through to the generic
"Unknown command" fallback (a single EID_INFO). This guards the v3 /time handler
that now emits the same two-line structure.

NOTE: the reply text is localized + charset-converted by the original under this
mock harness, so the exact strings are not byte-comparable — the line COUNT (and
EID code) is the faithful observable.

Run: python3 tests/diff/diff_time.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "TimeChan"
EID_INFO = 0x12


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.join_channel(alice, CHAN)
    events = bc.chat_command(alice, "/time")
    alice.close()
    return [eid for (eid, _u, _t) in events]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=10760)
    ap.add_argument("--v3-port", type=int, default=10772)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        o_info = [e for e in o if e == EID_INFO]
        n_info = [e for e in n if e == EID_INFO]
        print(f"oracle /time events: {[hex(e) for e in o]}")
        print(f"v3     /time events: {[hex(e) for e in n]}")
        ok = (len(o_info) == 2 and len(n_info) == 2 and o == n)
        if ok:
            print("OK: both emit two EID_INFO lines for /time.")
            return 0
        print("FAIL: /time reply structure diverged "
              f"(oracle {len(o_info)} INFO, v3 {len(n_info)} INFO).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
