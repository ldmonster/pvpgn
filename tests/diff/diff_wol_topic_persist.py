#!/usr/bin/env python3
"""Differential test: WOL channel TOPIC PERSISTS across an empty period.

In the original server, IRC/WOL channel topics live in a SEPARATE, channel-NAME-
keyed store (class_topiclist / topic.cpp) that is independent of the Channel
object's lifetime. When the last member of a non-permanent channel leaves (or
disconnects), the Channel object is destroyed but the topic SURVIVES in the
name-keyed store. A fresh re-joiner of the same channel name therefore gets the
old topic back (RPL_TOPIC 332).

v3 originally stored the topic ON the Channel domain object and removed the whole
channel when it emptied (destroy-on-empty), discarding the topic — so the
re-joiner saw an empty 332. This test guards the fix: a name-keyed in-memory
topic store that outlives the Channel.

Scenario (both servers, WOL/sku=1000):
  1. A joins #tpersist, sets `TOPIC #tpersist :<TOPIC>`, then DISCONNECTS
     (last member -> channel destroyed on both servers).
  2. Wait for cleanup.
  3. B joins #tpersist fresh and reads RPL_TOPIC 332.
Oracle and v3 must BOTH carry <TOPIC> in B's 332.

NOTE: a bare "TOPIC #chan" query CRASHES the original (NULL deref), so this test
never sends one.

Run: python3 tests/diff/diff_wol_topic_persist.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

CHAN = "tpersist"
TOPIC = "SecretTopic123"


def _drain(c, settle=0.6):
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
    # A sets the topic then disconnects -> channel empties and is destroyed.
    a = wc.wol_session(host, port, "tpa", "pw", sku=1000)
    if a is None:
        return None
    a.send_line(f"JOIN #{CHAN}")
    _drain(a)
    a.send_line(f"TOPIC #{CHAN} :{TOPIC}")
    _drain(a)
    a.close()  # last member leaves -> channel object destroyed on both servers
    time.sleep(1.2)  # allow cleanup
    # B re-joins fresh: does B's 332 carry the persisted topic?
    b = wc.wol_session(host, port, "tpb", "pw", sku=1000)
    if b is None:
        return None
    b.send_line(f"JOIN #{CHAN}")
    join_332 = _t332(_drain(b))
    b.close()
    return {"rejoin_332": join_332}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11980)
    ap.add_argument("--v3-port", type=int, default=12080)
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
        print(f"{'field':<14}{'oracle':<18}{'v3':<18}match")
        print("-" * 56)
        same = o["rejoin_332"] == n["rejoin_332"]
        print(f"{'rejoin_332':<14}{str(o['rejoin_332']):<18}"
              f"{str(n['rejoin_332']):<18}{'OK' if same else 'DIFF'}")
        print()
        success = same and o["rejoin_332"] == TOPIC
        if success:
            print("WOL TOPIC persists across destroy-on-empty (matches oracle).")
            return 0
        print("FAIL: WOL TOPIC persistence divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
