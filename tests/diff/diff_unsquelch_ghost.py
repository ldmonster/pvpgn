#!/usr/bin/env python3
"""Differential test: /unsquelch of a nonexistent user replies EID_INFO, not EID_ERROR.

The oracle splits the "No such user." miss reply by direction:
  - _handle_squelch_command   (src/bnetd/command.cpp) -> message_type_error -> EID_ERROR (0x13)
  - _handle_unsquelch_command (src/bnetd/command.cpp) -> message_type_info  -> EID_INFO  (0x12)

v3's shared BnetFsm::handle_squelch helper originally returned kEidError for the
miss in BOTH directions, so /unsquelch <ghost> wrongly came back as an error
event. The fix makes the miss event id direction-dependent. This guards it.

Scenario (against BOTH servers), per user, post full-login + JOIN:
  /unsquelch ghostuser999 -> CHATEVENT eid must be 0x12 (EID_INFO)
  /squelch   ghostuser888 -> CHATEVENT eid must be 0x13 (EID_ERROR)  [control]

Run: python3 tests/diff/diff_unsquelch_ghost.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "UnsquelchGhost"
USER = "ghostzara"
EID_INFO = 0x12
EID_ERROR = 0x13


def first_eid(evs):
    return evs[0][0] if evs else None


def scenario(host, port):
    c, _ = bc.full_login(host, port, USER, "pw")
    bc.join_channel(c, CHAN)
    bc.drain_chat(c)
    unsq = bc.chat_command(c, "/unsquelch ghostuser999")
    sq = bc.chat_command(c, "/squelch ghostuser888")
    try:
        c.close()
    except Exception:
        pass
    return {
        "unsquelch_eid": first_eid(unsq),
        "squelch_eid": first_eid(sq),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=12720)
    ap.add_argument("--v3-port", type=int, default=12726)
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
        for k in ("unsquelch_eid", "squelch_eid"):
            ov = hex(o[k]) if o[k] is not None else "None"
            nv = hex(n[k]) if n[k] is not None else "None"
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<16}{ov:<12}{nv:<12}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must demonstrate the direction split for the test to be
        # meaningful, and v3 must match it.
        oracle_ok = (o["unsquelch_eid"] == EID_INFO
                     and o["squelch_eid"] == EID_ERROR)
        if all_ok and oracle_ok:
            print("/unsquelch ghost -> EID_INFO, /squelch ghost -> EID_ERROR "
                  "(matches oracle).")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n}")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
