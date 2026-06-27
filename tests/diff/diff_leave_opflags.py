#!/usr/bin/env python3
"""Differential test: EID_LEAVE (0x03) carries the departing user's channel flags.

When a channel's tmpOP (operator) leaves or disconnects, the original broadcasts
the EID_LEAVE chat event to the remaining members with the leaving user's channel
flags in the FLAGS u32 (offset 4): MF_GAVEL=0x02 for the operator, 0x00 for a
normal member. v3 previously hardcoded flags=0 in the EID_LEAVE broadcast, so the
operator appeared to depart with flags=0.

Regression guard for bug-hunt wave 112 (follow-up to wave 58, which added the
gavel flag to JOIN/SHOWUSER/USERFLAGS but missed LEAVE).

Run: python3 tests/diff/diff_leave_opflags.py
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def _parse(body):
    if body is None or len(body) < 24:
        return None
    eid = struct.unpack_from("<I", body, 0)[0]
    flags = struct.unpack_from("<I", body, 4)[0]
    user = body[24:].split(b"\x00")[0].decode("latin-1", "replace")
    return eid, flags, user


def _join(c, chan, settle=0.6, collect=14):
    c.send(bc.SID_JOINCHANNEL, struct.pack("<I", 0) + bc.cstring(chan))
    time.sleep(settle)
    old = c.sock.gettimeout()
    c.sock.settimeout(0.4)
    try:
        for _ in range(collect):
            r = c.recv()
            if r is None:
                break
            if r[0] == bc.SID_PING:
                c.send(bc.SID_PING, r[1][:4])
    finally:
        c.sock.settimeout(old)


def _drain(c, settle=0.7, collect=14):
    time.sleep(settle)
    out = []
    old = c.sock.gettimeout()
    c.sock.settimeout(0.4)
    try:
        for _ in range(collect):
            r = c.recv()
            if r is None:
                break
            sid, body = r
            if sid == bc.SID_PING:
                c.send(bc.SID_PING, body[:4])
                continue
            if sid == bc.SID_CHATEVENT:
                ev = _parse(body)
                if ev:
                    out.append((hex(ev[0]), hex(ev[1]), ev[2]))
    finally:
        c.sock.settimeout(old)
    return out


def _flush(c):
    time.sleep(0.3)
    old = c.sock.gettimeout()
    c.sock.settimeout(0.3)
    try:
        while c.recv() is not None:
            pass
    except Exception:
        pass
    finally:
        c.sock.settimeout(old)


def scenario(host, port):
    # cobs is operator of LEAVEFL (observes normal-member leave).
    cobs, _ = bc.full_login(host, port, "lfobs", "pw")
    _join(cobs, "LEAVEFL")
    # aop becomes operator of fresh LEAVEFL2; obs2 joins to observe aop leaving.
    aop, _ = bc.full_login(host, port, "lfaop", "pw")
    _join(aop, "LEAVEFL2")
    obs2, _ = bc.full_login(host, port, "lfob2", "pw")
    _join(obs2, "LEAVEFL2")
    _flush(obs2)
    aop.close()  # operator disconnects
    op_leave = _drain(obs2)
    # normal member leave in LEAVEFL: bnorm joins (non-op), then leaves.
    bnorm, _ = bc.full_login(host, port, "lfbn", "pw")
    _join(bnorm, "LEAVEFL")
    _flush(cobs)
    bnorm.close()
    norm_leave = _drain(cobs)
    cobs.close()
    obs2.close()
    return {"operator_leave": op_leave, "normal_leave": norm_leave}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11240)
    ap.add_argument("--v3-port", type=int, default=11246)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"ORIGINAL: {o}")
        print(f"V3      : {n}")
        # Operator must depart with the gavel flag (0x2); a normal member with 0x0.
        expect_op = [("0x3", "0x2", "lfaop")]
        expect_norm = [("0x3", "0x0", "lfbn")]
        ok = (o["operator_leave"] == expect_op
              and o["normal_leave"] == expect_norm
              and n == o)
        if ok:
            print("PASS: EID_LEAVE operator gavel flag matches oracle")
            return 0
        print("FAIL: divergence in EID_LEAVE operator flag")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
