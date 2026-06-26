#!/usr/bin/env python3
"""Differential test: /whoami reports the caller's own location.

The original's _handle_whoami_command does do_whois(self) and replies with an
EID_INFO location report ("You are using Battle.net and are currently in channel
...". v3 previously had no /whoami handler, so it fell through to the generic
"Unknown command." error. The localized reply text is charset-garbled in this
harness (see diff_channelcmds.py), so the decisive, robust observable is the
reply KIND: both servers must answer /whoami with an EID_INFO event (not an
EID_ERROR / unknown-command).

Run: python3 tests/diff/diff_whoami.py
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
CHAN = "Loc"


def _kind(events):
    """First EID_INFO/EID_ERROR reply -> 'info'/'error'/'none'; flag unknown-cmd."""
    for (eid, _u, text) in events:
        if eid == EID_INFO:
            return ("info", "unknown command" in text.lower())
        if eid == EID_ERROR:
            return ("error", "unknown command" in text.lower())
    return ("none", False)


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "pw")
    bc.join_channel(alice, CHAN)
    bc.drain_chat(alice)
    res = _kind(bc.chat_command(alice, "/whoami"))
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
        print(f"oracle: kind={o[0]} unknown_cmd={o[1]}")
        print(f"v3    : kind={n[0]} unknown_cmd={n[1]}")
        # Decisive: both answer with EID_INFO and neither says "unknown command".
        success = (o[0] == "info" and n[0] == "info"
                   and not o[1] and not n[1])
        print()
        if success:
            print("/whoami matches the oracle (EID_INFO location report).")
            return 0
        print("FAIL: /whoami divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
