#!/usr/bin/env python3
"""Differential test: /who on a nonexistent channel emits TWO EID_ERROR lines.

The original's _handle_who_command (command.cpp) sends two message_type_error
(EID_ERROR 0x13) lines for an unknown channel: "That channel does not exist."
followed by the hint "(If you are trying to search for a user, use the /whois
command.)". v3 previously sent only the first line. The localized reply text is
charset-garbled in this harness, so the decisive, byte-stable observable is the
EID_ERROR packet count: both servers must answer "/who <bogus>" with exactly two
EID_ERROR chat events.

Run: python3 tests/diff/diff_who_nonexistent.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

EID_INFO = 0x12
EID_ERROR = 0x13
CHAN = "TestChan"


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "pw123456")
    bc.join_channel(alice, CHAN)
    bc.drain_chat(alice)
    events = bc.chat_command(alice, "/who NoSuchChannelXYZ", collect=12,
                             settle=0.6)
    alice.close()
    errs = sum(1 for (eid, _u, _t) in events if eid == EID_ERROR)
    infos = sum(1 for (eid, _u, _t) in events if eid == EID_INFO)
    return errs, infos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11960)
    ap.add_argument("--v3-port", type=int, default=11966)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o_err, o_info = scenario("127.0.0.1", args.orig_port)
        n_err, n_info = scenario("127.0.0.1", args.v3_port)
        print(f"oracle: EID_ERROR={o_err} EID_INFO={o_info}")
        print(f"v3    : EID_ERROR={n_err} EID_INFO={n_info}")
        # Decisive: both emit exactly two EID_ERROR and zero EID_INFO.
        success = (o_err == 2 and n_err == 2
                   and o_err == n_err and o_info == n_info)
        print()
        if success:
            print("/who <bogus> matches the oracle (two EID_ERROR lines).")
            return 0
        print("FAIL: /who nonexistent-channel EID_ERROR count divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
