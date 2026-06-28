#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""v3-conformance: D2DBS SAVE_DATA -> GET_DATA round-trip (allowladder + blob).

This is a v3-only conformance test, NOT differential: the original d2dbs only
serves GET_DATA for a character whose real .d2s save file exists on disk (its
SAVE path validates the .d2s structure via dbs_packet_fix_charinfo), so a
synthetic blob cannot be round-tripped through the oracle. The v3 in-memory
repository stores blobs opaquely, which lets us exercise the GET_DATA SUCCESS
path that the differential harness (which only tests a missing char) cannot.

Asserts the wave-217 fix: on a successful load the GET_DATA_REPLY reports
allowladder = 1 (with the default ladderinit_time = 0, every loaded character
is ladder-eligible, mirroring the oracle) instead of the old hardcoded 0, and
the saved blob round-trips byte-for-byte.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d2dbs_server import V3D2dbs  # noqa: E402
import d2dbs_client as dc  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--v3-d2dbs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2dbs/pvpgn_v3_d2dbs"))
    ap.add_argument("--v3-port", type=int, default=6262)
    a = ap.parse_args()

    blob = bytes(range(48))
    with V3D2dbs(a.v3_d2dbs, a.v3_port):
        time.sleep(0.3)
        c = dc.D2dbsClient("127.0.0.1", a.v3_port)
        try:
            c.send_init()
            time.sleep(0.15)
            c.save_data(seqno=1, account="acct", char="Hero", realm="r", blob=blob)
            sr = c.recv_frame(timeout=2.0)
            save_ok = (sr is not None and isinstance(sr[0], int) and
                       dc.parse_save_data_reply(sr[2])["result"] == dc.SAVE_DATA_SUCCESS)

            c.get_data(seqno=2, account="acct", char="Hero", realm="r")
            gr = c.recv_frame(timeout=2.0)
            parsed = (dc.parse_get_data_reply(gr[2])
                      if gr is not None and isinstance(gr[0], int) else None)
        finally:
            c.close()

    print(f"save_ok={save_ok}")
    print(f"get reply={parsed}")
    ok = (save_ok and parsed is not None and
          parsed["result"] == dc.GET_DATA_SUCCESS and
          parsed["allowladder"] == 1 and
          parsed["blob"] == blob and
          parsed["char_name"] == "Hero")
    print("OK: v3 d2dbs SAVE->GET round-trips with allowladder=1"
          if ok else "FAIL")
    return 0 if ok else 1


sys.exit(main())
