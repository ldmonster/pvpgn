#!/usr/bin/env python3
"""Differential test: WOL PRIVMSG (whisper) to a non-online user -> 401.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener), a
post-login whisper to a nick that is not an online user:

    PRIVMSG ghostuser :hello there

The original WOL handler (_handle_privmsg_command, handle_wol.cpp) takes the
whisper branch (target has no '#'), fails connlist_find_connection_by_accountname,
and replies:

    irc_send(conn, ERR_NOSUCHNICK, ":No such user")

which is the wire line:

    :<server> 401 <nick> :No such user

i.e. there is NO target middle param, and the trailing text is "No such user"
(NOT "No such nick"). v3 previously fed the target into send_numeric's trailing
text, yielding ":... 401 <nick> :<target> :No such nick" — a stray colon, the
target echoed, and the wrong text. This guard captures the raw 401 line, strips
the server name, and asserts v3 matches the oracle byte-for-byte.

(v3 has no nick->session whisper routing yet, so an online recipient also falls
to this miss path; the miss-form wire bytes are the cleanly-diffable part.)

Run: python3 tests/diff/diff_wol_privmsg_nouser.py
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _norm(line):
    """Strip the leading ':<server> ' so only the code-and-params remain."""
    if line is None:
        return None
    return re.sub(r"^:\S+\s+", "", line, count=1)


def scenario(host, port, sku=1000):
    a = wc.wol_session(host, port, "pmprober", "secretpass", sku=sku)
    if a is None:
        return None
    try:
        a.send_line("PRIVMSG ghostuser :hello there")
        for _ in range(25):
            line = a.read_line()
            if line is None:
                break
            if " 401 " in line:
                return _norm(line)
        return None
    finally:
        a.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6472)
    ap.add_argument("--v3-port", type=int, default=6572)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'':<8}{'401 line (server-name stripped)'}")
        print("-" * 60)
        print(f"{'oracle':<8}{o!r}")
        print(f"{'v3':<8}{n!r}")
        print()

        expect = "401 pmprober :No such user"
        if o == expect and n == expect:
            print("WOL PRIVMSG-to-ghost 401 matches the oracle "
                  "(':No such user', no target middle param).")
            return 0
        print("FAIL: WOL PRIVMSG-to-ghost 401 divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
