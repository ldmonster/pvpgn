#!/usr/bin/env python3
"""Differential test: WOL TOPIC for a channel the client is NOT on -> 442.

The original (_handle_topic_command, irc.cpp, WOL branch) persists the topic on
the client's CURRENT channel using the trailing text REGARDLESS of which channel
name was typed, then derives its reply from the query path: it answers
332 RPL_TOPIC only when the named channel matches the channel the client is on,
otherwise 442 ERR_NOTONCHANNEL "<name> :You're not on that channel".

v3 previously always answered 332 with the typed name (even for a foreign channel),
so a `TOPIC #other :x` while sitting in #lobby wrongly got a success echo. v3 now
mirrors the original: persist on the current channel, 332 on a name match, 442 on a
mismatch.

This checks both reply forms (same-channel -> 332, other-channel -> 442) and the
shared quirky side effect (the foreign-name SET still updates the current channel's
topic, visible to a later joiner's 332).

NOTE: a bare "TOPIC #chan" query (no trailing text) CRASHES the original
(NULL deref via std::string(NULL)), so it is never sent here.

Run: python3 tests/diff/diff_wol_topic_notonchan.py
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


def _numeric(lines, code):
    """Return the text after ' :' for the first line carrying numeric `code`."""
    needle = f" {code} "
    for l in lines:
        if needle in l:
            colon = l.find(" :")
            return l[colon + 2:] if colon >= 0 else ""
    return None


def scenario(host, port):
    a = wc.wol_session(host, port, "tnoa", "pw")
    if a is None:
        return None
    a.send_line("JOIN #Lobby")
    time.sleep(0.4)
    _drain(a)

    # TOPIC for a channel we are NOT on -> oracle answers 442 (and silently sets
    # the topic on our current channel #Lobby).
    a.send_line("TOPIC #Other :SneakySet")
    other = _drain(a)
    other_442 = _numeric(other, 442)
    other_332 = _numeric(other, 332)

    # TOPIC for our current channel -> 332 success echo.
    a.send_line("TOPIC #Lobby :RealTopic")
    same = _drain(a)
    same_332 = _numeric(same, 332)
    same_442 = _numeric(same, 442)

    a.close()
    return {
        "other_442": other_442 is not None,
        "other_332": other_332 is not None,
        "same_332": same_332,
        "same_442": same_442 is not None,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6414)
    ap.add_argument("--v3-port", type=int, default=6514)
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
        fields = ["other_442", "other_332", "same_332", "same_442"]
        print(f"{'field':<12}{'oracle':<14}{'v3':<14}match")
        print("-" * 50)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<12}{str(o[f]):<14}{str(n[f]):<14}{'OK' if same else 'DIFF'}")
        print()
        # Expected oracle shape: 442 (not 332) for the foreign channel, 332 for
        # the current channel.
        shape_ok = (o["other_442"] and not o["other_332"]
                    and o["same_332"] is not None and not o["same_442"])
        if all_ok and shape_ok:
            print("WOL TOPIC not-on-channel matches the oracle (442 vs 332).")
            return 0
        print("FAIL: WOL TOPIC not-on-channel divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
