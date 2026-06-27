#!/usr/bin/env python3
"""Differential test: BNCS SID_GETLADDERDATA (0x2E) paginated ladder snapshot.

Drives, against BOTH the original pvpgn-server and v3, after a full OLS login:

    A: SID_GETLADDERDATA  id=STANDARD(1) type=HIGHESTRATED(0) start=0 count=10
       ->  SERVER_LADDERREPLY echoing id/type/start/count + `count` rows

The original (_client_ladderreq) ALWAYS answers regardless of ladder backend:
it echoes the request header and emits `count` all-zero t_ladder_entry rows
(80 bytes each: current/active blocks + ttest[6] + 2x u64 lastgame), where
ttest[0] carries the row index i, each followed by a single space (" ") name.
With no ladder data this is fully deterministic, so the reply is byte-exact.

This is a byte-exact comparison (no timestamps/server-name in the body).

Run: python3 tests/diff/diff_ladderreq.py --v3-bnetd <path>
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_LADDER = 0x2E
USER = "ladderx"
# request: client_tag('SEXP'), id=STANDARD(1), type=HIGHESTRATED(0), start, count
START = 0
COUNT = 10


def recv_match(c, want, tries=25):
    for _ in range(tries):
        r = c.recv()
        if r is None:
            return None
        sid, body = r
        if sid == bc.SID_PING:
            c.send(bc.SID_PING, body[:4])
            continue
        if sid == want:
            return body
    return None


def scenario(host, port):
    try:
        c, _uniq = bc.full_login(host, port, USER, "ladderpass")
    except Exception as e:  # noqa: BLE001
        return {"err": str(e)}
    try:
        body = struct.pack("<IIIII", 0x53455850, 1, 0, START, COUNT)
        c.send(SID_LADDER, body)
        rb = recv_match(c, SID_LADDER)
        if rb is None:
            return {"err": "no SERVER_LADDERREPLY"}
        out = {"len": len(rb), "body": rb}
        if len(rb) >= 20:
            ct, idv, tp, sp, cnt = struct.unpack_from("<IIIII", rb, 0)
            out["id"], out["type"], out["start"], out["count"] = idv, tp, sp, cnt
        return out
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=12920)
    ap.add_argument("--v3-port", type=int, default=12926)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        if o.get("err") or n.get("err"):
            print(f"FAIL: error orig={o.get('err')} v3={n.get('err')}")
            return 1

        # Both must reply 0x2E, echo the header, and match the body byte-exact.
        hdr_fields = ("len", "id", "type", "start", "count")
        print(f"{'field':<8}{'original':<14}{'v3':<14}match")
        print("-" * 44)
        ok = True
        for f in hdr_fields:
            m = o.get(f) == n.get(f)
            ok = ok and m
            print(f"{f:<8}{str(o.get(f)):<14}{str(n.get(f)):<14}"
                  f"{'OK' if m else 'DIFF'}")
        body_match = o["body"] == n["body"]
        ok = ok and body_match
        print(f"{'body':<8}{'(' + str(len(o['body'])) + 'B)':<14}"
              f"{'(' + str(len(n['body'])) + 'B)':<14}"
              f"{'OK' if body_match else 'DIFF'}")
        print()

        want_hdr = {"id": 1, "type": 0, "start": START, "count": COUNT}
        got_hdr = {k: o.get(k) for k in want_hdr}
        if ok and got_hdr == want_hdr:
            print("BNCS SID_GETLADDERDATA matches the oracle "
                  "(0x2E reply, header echo, byte-exact rows).")
            return 0
        print(f"FAIL: SID_GETLADDERDATA divergence "
              f"(want header {want_hdr}, orig hdr={got_hdr}, "
              f"body_match={body_match}).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
