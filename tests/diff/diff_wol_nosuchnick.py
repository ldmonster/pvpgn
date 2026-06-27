#!/usr/bin/env python3
"""Differential test: WOL 401 ERR_NOSUCHNICK wire format (no stray colon).

Drives, against BOTH the original pvpgn-server and v3 (WOL listener), the four
WOL command handlers that reply with ERR_NOSUCHNICK when their target nick is
not an online user:

    USERIP <ghost>
    HOST <ghost> ...
    GAMEOPT <ghost> :opts
    ADDBUDDY <ghost>

The original (handle_wol.cpp) builds each as irc_send(ERR_NOSUCHNICK,
"<target> :No such nick"), so the wire line is:

    :<server> 401 <nick> <target> :No such nick

i.e. the target nick is a MIDDLE parameter — NO leading ':' before it. v3
previously fed the target into send_numeric's trailing-text field, which always
injects a " :", yielding ":... 401 <nick> :<target> :No such nick" (a stray
colon before the target). This guard captures the raw 401 line, strips the
server name, and asserts v3 matches the oracle byte-for-byte.

Run: python3 tests/diff/diff_wol_nosuchnick.py
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

CMDS = [
    ("USERIP", "USERIP ghostnick"),
    ("HOST", "HOST ghostnick somearg"),
    ("GAMEOPT", "GAMEOPT ghostnick :opts"),
    ("ADDBUDDY", "ADDBUDDY ghostnick"),
]


def _norm(line):
    """Strip the leading ':<server> ' so only the code-and-params remain."""
    if line is None:
        return None
    # ":<server> 401 nick target :No such nick" -> "401 nick target :No such nick"
    return re.sub(r"^:\S+\s+", "", line, count=1)


def scenario(host, port, sku=1000):
    out = {}
    for key, cmd in CMDS:
        a = wc.wol_session(host, port, "nsnprober", "secretpass", sku=sku)
        if a is None:
            out[key] = None
            continue
        try:
            a.send_line(cmd)
            got = None
            for _ in range(25):
                line = a.read_line()
                if line is None:
                    break
                if " 401 " in line:
                    got = line
                    break
            out[key] = _norm(got)
        finally:
            a.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6438)
    ap.add_argument("--v3-port", type=int, default=6538)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'cmd':<10}{'oracle':<34}{'v3':<34}match")
        print("-" * 90)
        ok = True
        for key, _ in CMDS:
            ov, nv = str(o[key]), str(n[key])
            m = ov == nv and o[key] is not None
            ok = ok and m
            print(f"{key:<10}{ov:<34}{nv:<34}{'OK' if m else 'DIFF'}")
        print()

        expect = "401 nsnprober ghostnick :No such nick"
        if ok and all(o[k] == expect for k, _ in CMDS):
            print("WOL ERR_NOSUCHNICK wire format matches the oracle "
                  "(target is a middle param, no stray colon).")
            return 0
        print("FAIL: WOL ERR_NOSUCHNICK divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
