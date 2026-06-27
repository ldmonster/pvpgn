#!/usr/bin/env python3
"""Differential test: WOL JOIN of a non-'#'-prefixed channel name.

The original (irc.cpp _handle_join_command) runs the JOIN target through
irc_convert_ircname(), which returns NULL for any name not prefixed with '#'
(or '!<id>'). A NULL ircname makes the server answer ERR_NOSUCHCHANNEL:

    :<server> 403 <nick> <name> :JOIN failed

v3 previously stripped a leading '#' (a no-op for a bare name) and auto-created
a brand-new channel from the token, then echoed a full JOIN/353/332/366. That
let a malformed JOIN spawn junk channels the original would have rejected.
WolFsm::on_join now mirrors the original: a non-'#' name (other than the RFC
"JOIN 0" part-all sentinel) gets 403 "JOIN failed".

A normal '#' JOIN must still succeed (echo + 366), so we verify both.

Run: python3 tests/diff/diff_wol_join_unprefixed.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.7):
    time.sleep(settle)
    c.sock.settimeout(0.5)
    out = []
    try:
        while True:
            line = c.read_line()
            if line is None:
                break
            out.append(line)
    except OSError:
        pass
    return out


def _code_and_tail(lines):
    """Return (numeric, tail-after-server-name) of the first numeric reply line,
    or (None, None). The ':<servername> ' prefix is stripped so the comparison
    is server-name independent."""
    for l in lines:
        code = wc.WolClient.numeric(l)
        if code is not None:
            sp = l.find(" ")
            tail = l[sp + 1:] if sp >= 0 else l
            return code, tail
    return None, None


def scenario(host, port):
    a = wc.wol_session(host, port, "uj", "pw")
    if a is None:
        return None
    # 1) unprefixed JOIN -> expect 403 ... <name> :JOIN failed
    a.send_line("JOIN plainname")
    unpref = _code_and_tail(_drain(a))
    # 2) normal '#' JOIN must still work -> echo JOIN + 366 end-of-names
    a.send_line("JOIN #realchan")
    good_lines = _drain(a)
    joined = any(" JOIN " in l for l in good_lines)
    end366 = any(wc.WolClient.numeric(l) == 366 for l in good_lines)
    a.close()
    return {
        "unpref_code": unpref[0],
        "unpref_line": unpref[1],
        "good_join": joined,
        "good_366": end366,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=13760)
    ap.add_argument("--v3-port", type=int, default=13772)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        if not o or not n:
            print("FAIL: login/setup failed")
            return 1
        fields = ["unpref_code", "unpref_line", "good_join", "good_366"]
        print(f"{'field':<14}{'oracle':<40}{'v3':<40}match")
        print("-" * 110)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<14}{str(o[f]):<40}{str(n[f]):<40}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["unpref_code"] == 403
                   and o["good_join"] and o["good_366"])
        if success:
            print("WOL JOIN unprefixed-name matches the oracle (403 JOIN failed; "
                  "'#' join still works).")
            return 0
        print("FAIL: WOL JOIN unprefixed-name divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
