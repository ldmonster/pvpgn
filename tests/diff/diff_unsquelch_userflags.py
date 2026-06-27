#!/usr/bin/env python3
"""Differential test: a successful /unsquelch of an ONLINE user emits a trailing
EID_USERFLAGS naming that user.

The original `_handle_unsquelch_command` (command.cpp), on the removal-succeeded
branch (conn_del_ignore() >= 0), sends EID_INFO "No longer ignoring." and then,
ONLY when the target is currently online (account_get_conn(target) != NULL),
a trailing EID_USERFLAGS (0x09) carrying the now-unignored user's chat name,
channel flags and statstring as text (message_type_userflags).

It is gated strictly:
  - fired only on the success branch (NOT "User was not being ignored."),
  - and only for an online target.

v3 previously sent only the EID_INFO and omitted the EID_USERFLAGS. This guards
that v3 now matches: structure compared (eid kinds + the EID_USERFLAGS username),
statstring text and any non-zero gavel flags are normalized away.

Scenario (against BOTH servers):
  alice, bob: login, JOIN #chan (both online)
  alice /squelch bob ; drain
  alice /unsquelch bob  -> EID_INFO(0x12) then EID_USERFLAGS(0x09) un='bob'
  alice /unsquelch bob  (now not ignored) -> EID_INFO(0x12) only, NO 0x09

Run: python3 tests/diff/diff_unsquelch_userflags.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "UnsqUF"
EID_INFO = 0x12
EID_USERFLAGS = 0x09


def scenario(host, port):
    alice, _ = bc.full_login(host, port, "alice", "apw")
    bob, _ = bc.full_login(host, port, "bob", "bpw")
    bc.join_channel(alice, CHAN)
    bc.join_channel(bob, CHAN)
    bc.drain_chat(alice)
    bc.drain_chat(bob)

    # squelch bob, drain the "has been squelched." notice
    bc.chat_command(alice, "/squelch bob")
    bc.drain_chat(alice)

    # first unsquelch: bob is online + was ignored -> success + EID_USERFLAGS
    first = bc.chat_command(alice, "/unsquelch bob")
    time.sleep(0.3)
    first += bc.drain_chat(alice)

    # second unsquelch: bob no longer ignored -> NO EID_USERFLAGS
    second = bc.chat_command(alice, "/unsquelch bob")
    time.sleep(0.3)
    second += bc.drain_chat(alice)

    for c in (alice, bob):
        try:
            c.close()
        except Exception:
            pass

    first_uf = [e for e in first if e[0] == EID_USERFLAGS]
    return {
        # success branch: an EID_INFO present and an EID_USERFLAGS naming bob
        "first_has_info": any(e[0] == EID_INFO for e in first),
        "first_uf_count": len(first_uf),
        "first_uf_names_bob": any("bob" in (e[1] or "").lower()
                                  for e in first_uf),
        # not-ignored branch: NO EID_USERFLAGS at all
        "second_uf_count": len([e for e in second if e[0] == EID_USERFLAGS]),
        "second_has_info": any(e[0] == EID_INFO for e in second),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=13880)
    ap.add_argument("--v3-port", type=int, default=13886)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"{'field':<22}{'original':<12}{'v3':<12}match")
        print("-" * 58)
        all_ok = True
        for k in ("first_has_info", "first_uf_count", "first_uf_names_bob",
                  "second_uf_count", "second_has_info"):
            ov, nv = str(o[k]), str(n[k])
            same = ov == nv
            all_ok &= same
            print(f"{k:<22}{ov:<12}{nv:<12}{'OK' if same else 'DIFF'}")
        print()
        # The oracle must demonstrate the behavior (one EID_USERFLAGS naming bob
        # on the success branch, none on the not-ignored branch) for the test to
        # be meaningful, and v3 must match it.
        oracle_ok = (o["first_uf_count"] == 1 and o["first_uf_names_bob"]
                     and o["second_uf_count"] == 0)
        if all_ok and oracle_ok:
            print("Online /unsquelch emits trailing EID_USERFLAGS "
                  "(matches oracle).")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n}")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
