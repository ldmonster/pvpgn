#!/usr/bin/env python3
"""Differential test: an UNKNOWN slash-command is an ERROR, not an info notice.

The original routes command failures through message_send_text(..., the
message_type_error path, ...) which the bnet formatter (message.cpp
message_bnet_format case message_type_error) emits as SERVER_MESSAGE_TYPE_ERROR
(EID_ERROR 0x13) with flags=0 and an EMPTY username field. Successful command
*output* instead goes out as message_type_info (EID_INFO 0x12). v3 had collapsed
both into a single EID_INFO reply (username="Battle.net"), so a real client saw
an unknown command as an informational notice rather than an error.

Regression guard for bug-hunt wave 92: `/nonexistentcmd` must come back as
EID_ERROR (0x13) with an empty username on BOTH servers. The localized reply text
is charset-garbled in this harness, so the decisive observables are the event
KIND (error vs info) and the empty username — not the exact string.

Run: python3 tests/diff/diff_unknown_command.py
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
CHAN = "Unk"


def _first_reply(events):
    """First EID_INFO/EID_ERROR -> (kind, username); else ('none', '')."""
    for (eid, user, _text) in events:
        if eid == EID_INFO:
            return ("info", user)
        if eid == EID_ERROR:
            return ("error", user)
    return ("none", "")


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "pw")
    bc.enter_chat(alice, "alice")
    bc.join_channel(alice, CHAN)
    bc.drain_chat(alice)
    res = _first_reply(bc.chat_command(alice, "/nonexistentcmd"))
    alice.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6390)
    ap.add_argument("--v3-port", type=int, default=6490)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"oracle: kind={o[0]} username={o[1]!r}")
        print(f"v3    : kind={n[0]} username={n[1]!r}")
        print()
        # Decisive: both answer with EID_ERROR and an empty username.
        success = (o == n and o[0] == "error" and o[1] == "")
        if success:
            print("unknown-command matches the oracle (EID_ERROR, empty username).")
            return 0
        print("FAIL: unknown-command divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
