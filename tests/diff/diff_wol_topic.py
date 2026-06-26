#!/usr/bin/env python3
"""Differential test: WOL TOPIC (set + persist + 332 on join).

TOPIC was a silent no-op in v3 (wol_known[]). The original stores a channel topic
on "TOPIC #chan :text", echoes 332 RPL_TOPIC to the setter, and includes the topic
(332) in every later JOIN. v3 now implements WolFsm::on_topic (persist via the
SetChannelTopic use-case) + emits 332 on join.

This verifies the full chain end-to-end: A sets the topic, A gets the 332 echo, and
B (joining afterwards) sees the persisted topic in their JOIN.

NOTE: a bare "TOPIC #chan" query CRASHES the original (NULL deref), so this test
never sends one. v3 handles the query safely, but it is not exercised here.

Run: python3 tests/diff/diff_wol_topic.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

TOPIC = "HelloTopic"


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
    a = wc.wol_session(host, port, "topa", "pw")
    if a is None:
        return None
    a.send_line("JOIN #T")
    time.sleep(0.4)
    _drain(a)
    a.send_line(f"TOPIC #T :{TOPIC}")
    set_echo = _t332(_drain(a))
    b = wc.wol_session(host, port, "topb", "pw")
    if b is None:
        a.close()
        return None
    b.send_line("JOIN #T")
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
    ap.add_argument("--orig-port", type=int, default=6406)
    ap.add_argument("--v3-port", type=int, default=6506)
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
        success = all_ok and o["set_echo"] == TOPIC and o["join_332"] == TOPIC
        if success:
            print("WOL TOPIC matches the oracle (set echo + persisted topic on join).")
            return 0
        print("FAIL: WOL TOPIC divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
