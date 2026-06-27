#!/usr/bin/env python3
"""Differential test: /version and /copyright are real EID_INFO replies.

Drives, against BOTH the original pvpgn-server and v3:

    alice logs in, joins a channel, sends /version then /copyright.

The original routes these through the CommandRegistry
(_handle_version_command / _handle_copyright_command):

  /version    -> one  EID_INFO (0x12) line "<software> <version>"
                 (PVPGN_SOFTWARE " " PVPGN_VERSION, plain ASCII).
  /copyright  -> 15x  EID_INFO (0x12) fixed plain-ASCII lines
                 (also reachable via /warranty and /license).

v3 never wired its command_registry, so before this fix BOTH commands fell
through to the dead-registry fallback and answered with a single EID_ERROR
(0x13) "Unknown command." line. The decisive observable is the chat-event EID:
0x12 (real output) vs 0x13 (unknown-command error).

What is compared (STRUCTURE, not server-specific values):
  * /version  : exactly one event, EID == 0x12, text begins "PvPGN ".
                (The version *number* legitimately differs between the oracle
                and v3, so the number is not byte-compared.)
  * /copyright: the EID sequence is identical on both servers AND the text is
                byte-for-byte identical (these lines are a fixed ASCII table,
                not localized, so they ARE fully diffable).

Run: python3 tests/diff/diff_version.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "VersionChan"
EID_INFO = 0x12
EID_ERROR = 0x13


def run_cmd(host, port, cmd, collect=24):
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bc.join_channel(alice, CHAN)
    events = bc.chat_command(alice, cmd, collect=collect)
    alice.close()
    return events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=12480)
    ap.add_argument("--v3-port", type=int, default=12492)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    failures = []
    try:
        orig.start()
        v3.start()

        # ---- /version --------------------------------------------------
        o_ver = run_cmd("127.0.0.1", args.orig_port, "/version")
        n_ver = run_cmd("127.0.0.1", args.v3_port, "/version")
        print("=== /version ===")
        print(f"  oracle: {[(hex(e), t) for (e, _u, t) in o_ver]}")
        print(f"  v3    : {[(hex(e), t) for (e, _u, t) in n_ver]}")
        o_eids = [e for (e, _u, _t) in o_ver]
        n_eids = [e for (e, _u, _t) in n_ver]
        if o_eids != [EID_INFO]:
            failures.append(f"/version oracle EID structure unexpected: "
                            f"{[hex(e) for e in o_eids]}")
        if n_eids != [EID_INFO]:
            failures.append(f"/version v3 EID structure {[hex(e) for e in n_eids]} "
                            f"!= [0x12] (still unknown-command?)")
        else:
            v3_text = n_ver[0][2]
            if not v3_text.startswith("PvPGN "):
                failures.append(f"/version v3 text {v3_text!r} does not begin "
                                f"'PvPGN '")
        if o_ver and not o_ver[0][2].startswith("PvPGN "):
            failures.append(f"/version oracle text {o_ver[0][2]!r} unexpected")

        # ---- /copyright (fixed ASCII -> fully byte-diffable) ------------
        o_cr = run_cmd("127.0.0.1", args.orig_port, "/copyright")
        n_cr = run_cmd("127.0.0.1", args.v3_port, "/copyright")
        print("\n=== /copyright ===")
        print(f"  oracle: {len(o_cr)} lines, EIDs "
              f"{set(hex(e) for (e, _u, _t) in o_cr)}")
        print(f"  v3    : {len(n_cr)} lines, EIDs "
              f"{set(hex(e) for (e, _u, _t) in n_cr)}")
        o_pairs = [(e, t) for (e, _u, t) in o_cr]
        n_pairs = [(e, t) for (e, _u, t) in n_cr]
        if not o_pairs or any(e != EID_INFO for (e, _t) in o_pairs):
            failures.append(f"/copyright oracle not all EID_INFO: "
                            f"{[hex(e) for (e, _t) in o_pairs]}")
        if o_pairs != n_pairs:
            failures.append("/copyright text/EID sequence diverges:")
            for i in range(max(len(o_pairs), len(n_pairs))):
                op = o_pairs[i] if i < len(o_pairs) else None
                npp = n_pairs[i] if i < len(n_pairs) else None
                if op != npp:
                    failures.append(f"    line {i}: oracle {op!r} vs v3 {npp!r}")

        if failures:
            print("\nFAIL:")
            for f in failures:
                print("  -", f)
            return 1
        print("\nOK: /version and /copyright match the oracle (EID 0x12; "
              "copyright byte-identical).")
        return 0
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
