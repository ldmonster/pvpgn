#!/usr/bin/env python3
"""Differential test: WOL TOPIC with an extra middle param uses the trailing text.

A real IRC line is "<cmd> <middle params...> :<trailing>". The original's line
tokeniser (handle_irc_common.cpp) splits a TOPIC line into middle params plus a
single trailing ":" parameter, and _handle_topic_command sets the channel topic
from that *trailing* text alone — any extra middle params between the channel
name and the ":" are discarded. So:

    TOPIC #T extra :TheTopic    -> topic becomes "TheTopic"  (not "extra :TheTopic")

v3 previously took everything after the channel token as the topic, so it set the
topic to "extra :TheTopic" and echoed that back in 332 — a divergence. v3 now
extracts the IRC trailing parameter (text after the first " :").

This drives the extra-middle-param form against BOTH servers and checks the 332
set echo AND the persisted topic seen by a later joiner. The normal single-param
form is already covered by diff_wol_topic.py.

NOTE: a bare/colon-less "TOPIC #chan" CRASHES the original (NULL deref), so this
test never sends one.

Run: python3 tests/diff/diff_wol_topic_extraparam.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

TOPIC = "TheTopic"


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


def _t332(lines):
    for l in lines:
        if " 332 " in l:
            colon = l.find(" :")
            return l[colon + 2:] if colon >= 0 else ""
    return None


def scenario(host, port):
    a = wc.wol_session(host, port, "topxa", "pw")
    if a is None:
        return None
    a.send_line("JOIN #TX")
    time.sleep(0.4)
    _drain(a)
    # Extra middle param ("extra") between the channel name and the trailing ":".
    a.send_line(f"TOPIC #TX extra :{TOPIC}")
    set_echo = _t332(_drain(a))
    b = wc.wol_session(host, port, "topxb", "pw")
    if b is None:
        a.close()
        return None
    b.send_line("JOIN #TX")
    join_332 = _t332(_drain(b))
    a.close()
    b.close()
    return {"set_echo": set_echo, "join_332": join_332}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6408)
    ap.add_argument("--v3-port", type=int, default=6508)
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
        fields = ["set_echo", "join_332"]
        print(f"{'field':<12}{'oracle':<14}{'v3':<14}match")
        print("-" * 48)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<12}{str(o[f]):<14}{str(n[f]):<14}{'OK' if same else 'DIFF'}")
        print()
        # The trailing text only ("TheTopic"), with the "extra" middle param
        # discarded, must be what both servers store and echo.
        success = all_ok and o["set_echo"] == TOPIC and o["join_332"] == TOPIC
        if success:
            print("WOL TOPIC extra-middle-param matches the oracle "
                  "(trailing text used, middle param discarded).")
            return 0
        print("FAIL: WOL TOPIC extra-middle-param divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
