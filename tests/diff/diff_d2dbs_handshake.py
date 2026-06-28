#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Differential test: v3 d2dbs vs. original pvpgn d2dbs.

A mock D2GS opens the connection (0x65 connect-class byte), then issues
GET_DATA requests for characters that do not exist. Both servers must
reply with a byte-identical GET_DATA_REPLY carrying result=FAILED.

This exercises the v3 d2dbs end-to-end: connect-class handshake, 8-byte
framing, GET_DATA request parse, and the GET_DATA_REPLY encoder (which
echoes seqno/datatype/char_name captured per-request by the session).

Note on sequencing: the original d2dbs advances exactly one connection
"phase" per socket read event — it consumes the connect-class byte on the
first read, and only processes framed packets on a subsequent read. So the
client sends the init byte, flushes, briefly pauses, then sends the request
(faithful to how a real D2GS connects). v3 is read-boundary agnostic.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import d2dbs_client as dc
from d2dbs_server import V3D2dbs, OriginalD2dbs


def getdata_roundtrip(host, port, account, char, realm, datatype, seqno):
    """Connect as a mock D2GS, GET a (missing) char, return (type, seqno, body)."""
    c = dc.D2dbsClient(host, port)
    try:
        c.send_init()
        time.sleep(0.15)  # let the oracle consume the connect byte first
        c.get_data(seqno=seqno, account=account, char=char, realm=realm,
                   datatype=datatype)
        return c.recv_frame(timeout=3.0)
    finally:
        c.close()


SCENARIOS = [
    # (label, account, char, realm, datatype, seqno)
    ("charsave-missing", "nobody", "ghost", "", dc.DATATYPE_CHARSAVE, 0x1234),
    ("portrait-missing", "nobody", "ghost", "", dc.DATATYPE_PORTRAIT, 0x5678),
    ("charsave-realm",   "acct",   "hero",  "TestRealm", dc.DATATYPE_CHARSAVE, 0x00ABCDEF),
]


def frame_repr(fr):
    if fr is None:
        return "<no frame>"
    ptype, seqno, body = fr
    pt = ptype if isinstance(ptype, str) else hex(ptype)
    sq = seqno if isinstance(seqno, str) else hex(seqno)
    return f"type={pt} seqno={sq} body={body.hex()}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2dbs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2dbs/pvpgn_v3_d2dbs"))
    ap.add_argument("--orig-port", type=int, default=6124)
    ap.add_argument("--v3-port", type=int, default=6224)
    a = ap.parse_args()

    orig = OriginalD2dbs(a.orig_repo, a.orig_port)
    v3 = V3D2dbs(a.v3_d2dbs, a.v3_port)
    failures = 0
    try:
        orig.start()
        v3.start()

        for label, account, char, realm, datatype, seqno in SCENARIOS:
            o = getdata_roundtrip("127.0.0.1", a.orig_port,
                                  account, char, realm, datatype, seqno)
            n = getdata_roundtrip("127.0.0.1", a.v3_port,
                                  account, char, realm, datatype, seqno)
            same = (o is not None and n is not None and o == n)
            status = "OK" if same else "DIFF"
            print(f"[{status}] {label}")
            print(f"        oracle: {frame_repr(o)}")
            print(f"        v3    : {frame_repr(n)}")
            if same:
                parsed = dc.parse_get_data_reply(o[2])
                if not parsed or parsed["result"] != dc.GET_DATA_FAILED:
                    print(f"        !! expected result=FAILED, got {parsed}")
                    failures += 1
            else:
                failures += 1

        if failures == 0:
            print("OK: v3 d2dbs GET_DATA replies are byte-identical to the oracle")
        else:
            print(f"FAIL: {failures} scenario(s) diverged")
        return 0 if failures == 0 else 1
    finally:
        v3.stop()
        orig.stop()


sys.exit(main())
