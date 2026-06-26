#!/usr/bin/env python3
"""Differential test: WOL SQUADINFO / CLANBYNAME (clan info lookup).

NOTE ON SCOPE: this test was written while investigating WOL end-of-game
reporting (GAMERES). GAMERES turned out NOT to be an IRC verb at all — the
original implements it as a *binary* packet protocol on a separate listener
(conn_class_wgameres, port 4807; handle_wol_gameres.cpp), which the diff harness
(WOL IRC listener only) cannot drive, and whose effects are ladder/stats writes
that need a backend. So GAMERES is not cleanly diffable here. While mapping the
WOL verb surface, SQUADINFO and CLANBYNAME surfaced as real-handler-vs-no-op
divergences, which ARE cleanly diffable — that is what this test covers.

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

    client: WOL login
    SQUADINFO              -> oracle: 461 ERR_NEEDMOREPARAMS
    SQUADINFO 0            -> oracle: 439 ERR_IDNOEXIST  (caller has no clan)
    CLANBYNAME             -> oracle: 461 ERR_NEEDMOREPARAMS
    CLANBYNAME <nick>      -> oracle: 439 ERR_IDNOEXIST  (target has no clan)

None of these need a clan/stats backend: the empty-param paths return 461, and a
freshly auto-created account has no clan so the lookup paths return 439.

OBSERVED DIVERGENCE (as of this writing): v3 replies with SILENCE for all four.
v3 lists SQUADINFO and CLANBYNAME in the `wol_known` "silently accept" array in
src/protocol/wol/src/wol_fsm.cpp (dispatch_line) and returns core::ok() without
emitting any numeric. The original (src/bnetd/handle_wol.cpp,
_handle_squadinfo_command @749 / _handle_clanbyname_command @770) emits the
461/439 numerics above.

The test passes when v3 reproduces the oracle's reply codes; it currently FAILS,
documenting the missing v3 clan-info handlers.

Run: python3 tests/diff/diff_wol_squadinfo.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

FIELDS = ("squadinfo_noparam", "squadinfo_0", "clanbyname_noparam", "clanbyname_nick")
COMMANDS = {
    "squadinfo_noparam": "SQUADINFO",
    "squadinfo_0": "SQUADINFO 0",
    "clanbyname_noparam": "CLANBYNAME",
    "clanbyname_nick": "CLANBYNAME clanprobe",
}


def _probe(client, cmd, short_timeout=2.0):
    """Send cmd, read one reply; return its IRC numeric (or None if silent)."""
    client.sock.settimeout(short_timeout)
    client.send_line(cmd)
    line = client.read_line()
    return wc.WolClient.numeric(line) if line else None


def scenario(host, port, sku=1000):
    """Returns {field: numeric-code-or-None} for each clan-info probe."""
    res = {k: None for k in FIELDS}
    c = wc.wol_session(host, port, "clanprobe", "secretpass", sku=sku)
    if c is None:
        return res
    try:
        for k in FIELDS:
            res[k] = _probe(c, COMMANDS[k])
    finally:
        c.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6398)
    ap.add_argument("--v3-port", type=int, default=6498)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<20}{'oracle':<10}{'v3':<10}match")
        print("-" * 48)
        ok = True
        for k in FIELDS:
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<20}{ov:<10}{nv:<10}{'OK' if m else 'DIFF'}")
        print()

        # Sanity: the oracle must actually have produced the documented codes,
        # else the harness/env changed and the comparison is meaningless.
        oracle_ok = (o["squadinfo_noparam"] == 461 and o["squadinfo_0"] == 439 and
                     o["clanbyname_noparam"] == 461 and o["clanbyname_nick"] == 439)
        if not oracle_ok:
            print("FAIL: oracle did not produce the expected SQUADINFO/CLANBYNAME "
                  "reply codes; harness/env drift?")
            return 2
        if ok:
            print("WOL SQUADINFO/CLANBYNAME matches the oracle.")
            return 0
        print("FAIL: WOL SQUADINFO/CLANBYNAME divergence (v3 silently no-ops "
              "verbs the original answers with 461/439).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
