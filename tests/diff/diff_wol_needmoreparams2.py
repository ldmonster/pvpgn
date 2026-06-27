#!/usr/bin/env python3
"""Differential test: WOL 461 ERR_NEEDMOREPARAMS for HOST / USERIP / INVMSG.

A companion to diff_wol_needmoreparams.py, covering three parameter-requiring
verbs that test did NOT exercise:

    HOST    requires >=1 param  (_handle_host_command)
    USERIP  requires >=1 param  (_handle_userip_command)
    INVMSG  requires >=3 params (_handle_invmsg_command)

The original pvpgn-server replies to each, when sent with too few parameters,
with the standard:

    :<server> 461 <nick> <CMD> :Not enough parameters

OBSERVED DIVERGENCE (before this wave): v3's on_host/on_userip/on_invmsg
short-circuited the no-/few-param case with a silent `return core::ok()`
("original guard"), so a real client got NOTHING back where the oracle sends a
461. This test sends each command with deliberately-insufficient params to BOTH
servers, captures the 461 line, normalises away the (legitimately differing)
server-name prefix, and asserts the remainder matches byte-for-byte.

Run: python3 tests/diff/diff_wol_needmoreparams2.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

# (command-as-sent, expected-CMD-token). HOST/USERIP bare; INVMSG with 2 params
# (one short of the required 3) to exercise the "few params" branch.
PROBES = [
    ("HOST", "HOST"),
    ("USERIP", "USERIP"),
    ("INVMSG #room 1", "INVMSG"),
]


def _strip_server(line):
    if line.startswith(":"):
        parts = line.split(" ", 1)
        return parts[1] if len(parts) > 1 else line
    return line


def _probe(host, wolport, cmd):
    c = wc.wol_session(host, wolport, "alice", "pw")
    if c is None:
        return "<login-failed>"
    try:
        c.sock.settimeout(0.8)
        c.send_line(cmd)
        for _ in range(10):
            line = c.read_line()
            if line is None:
                break
            if wc.WolClient.numeric(line) == 461:
                return _strip_server(line)
        return None
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6710)
    ap.add_argument("--v3-port", type=int, default=6722)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        print(f"{'command':<18}{'oracle 461 (stripped)':<40}"
              f"{'v3':<40}match")
        print("-" * 110)
        all_ok = True
        for cmd, _tok in PROBES:
            o = _probe("127.0.0.1", orig.wolv1_port, cmd)
            n = _probe("127.0.0.1", v3.wol_port, cmd)
            same = (o == n)
            all_ok &= same
            print(f"{cmd:<18}{str(o):<40}{str(n):<40}"
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("WOL HOST/USERIP/INVMSG 461 matches the oracle.")
            return 0
        print("FAIL: WOL HOST/USERIP/INVMSG 461 diverges from the oracle.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
