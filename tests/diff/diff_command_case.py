#!/usr/bin/env python3
"""Differential test: chat slash-command dispatch is case-INsensitive.

Drives, against BOTH the original pvpgn-server and v3:

    alice logs in, joins a channel, sends a slash-command in several casings.

The original dispatches chat slash-commands case-insensitively: command.cpp's
strstart() compares with strncasecmp() (util.cpp), so "/TIME", "/Time" and
"/time" all route to the same handler. v3 previously matched the command name
case-SENSITIVELY, so any non-lowercase spelling fell through to the
"Unknown command" EID_ERROR (0x13) fallback instead of running the handler.

Observable compared: the STRUCTURE of the reply — the sequence of chat-event
EID codes. For a known command, every casing must produce the SAME EID
structure as the canonical lowercase spelling (and it must NOT be the
EID_ERROR unknown-command reply).

  /time / /TIME / /Time  -> two EID_INFO (0x12) lines on a bnet-class conn
  /whoami / /WHOAMI      -> one EID_INFO (0x12) whois-self line

NOTE: reply text is localized + charset-converted by the original under this
mock harness, so exact strings are not byte-comparable — the EID structure is
the faithful observable.

Run: python3 tests/diff/diff_command_case.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "CaseChan"
EID_INFO = 0x12
EID_ERROR = 0x13

# command -> list of casings that must all match the first (canonical) casing.
CASES = {
    "time": ["/time", "/TIME", "/Time"],
    "whoami": ["/whoami", "/WHOAMI", "/WhoAmI"],
}


def scenario(host, port, cmd):
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.join_channel(alice, CHAN)
    events = bc.chat_command(alice, cmd)
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
    ap.add_argument("--orig-port", type=int, default=11360)
    ap.add_argument("--v3-port", type=int, default=11372)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    failures = []
    try:
        orig.start()
        v3.start()
        for name, casings in CASES.items():
            canonical = casings[0]
            o_canon = scenario("127.0.0.1", args.orig_port, canonical)
            n_canon = scenario("127.0.0.1", args.v3_port, canonical)
            print(f"\n=== command {name!r} canonical {canonical!r} ===")
            print(f"  oracle: {[hex(e) for e in o_canon]}")
            print(f"  v3    : {[hex(e) for e in n_canon]}")
            if o_canon != n_canon:
                failures.append(
                    f"{canonical}: canonical structure diverges "
                    f"(oracle {[hex(e) for e in o_canon]} vs "
                    f"v3 {[hex(e) for e in n_canon]})")
                continue
            if EID_ERROR in n_canon:
                failures.append(f"{canonical}: canonical resolved to EID_ERROR")
                continue
            for variant in casings[1:]:
                o_var = scenario("127.0.0.1", args.orig_port, variant)
                n_var = scenario("127.0.0.1", args.v3_port, variant)
                print(f"  {variant!r}: oracle {[hex(e) for e in o_var]}  "
                      f"v3 {[hex(e) for e in n_var]}")
                # Oracle: variant must match canonical (case-insensitive).
                if o_var != o_canon:
                    failures.append(
                        f"{variant}: oracle variant != canonical "
                        f"({[hex(e) for e in o_var]} vs "
                        f"{[hex(e) for e in o_canon]})")
                # v3: variant must match oracle (and thus canonical).
                if n_var != o_var:
                    failures.append(
                        f"{variant}: v3 {[hex(e) for e in n_var]} != "
                        f"oracle {[hex(e) for e in o_var]}")
                if EID_ERROR in n_var:
                    failures.append(
                        f"{variant}: v3 returned EID_ERROR unknown-command")
        if failures:
            print("\nFAIL:")
            for f in failures:
                print("  -", f)
            return 1
        print("\nOK: all casings dispatch identically on both servers.")
        return 0
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
