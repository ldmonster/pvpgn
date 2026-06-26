#!/usr/bin/env python3
"""Hardening regression guard: the server survives a malformed-input battery.

Fleet robustness probes (BNCS + WOL, ~100 hostile cases) found v3 has NO crash or
hang: it caps buffers like the oracle, tolerates count-overflow array fields,
lying length prefixes, oversized bodies, floods, binary/NUL payloads, and
pre-login/out-of-order commands. This test pins that guarantee: it fires a
representative malformed-input battery at BOTH the oracle and v3, then asserts
each server is still ALIVE by completing a fresh, valid BNCS handshake and WOL
login afterwards. A regression that introduced a crash/hang would fail here.

(Behavioral DoS-resistance divergences — v3 keeps misbehaving connections open
where the oracle proactively closes them — are documented in
bug-hunt/findings/malformed-input-safety.md, not asserted here.)

Run: python3 tests/diff/diff_robustness.py
"""
import argparse
import os
import socket
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402
import wol_client as wc  # noqa: E402


def _raw_bncs(host, port, payload):
    """Open a BNCS connection (init byte), send raw bytes, close. Never raises."""
    try:
        s = socket.create_connection((host, port), timeout=3.0)
        s.settimeout(3.0)
        s.sendall(b"\x01")  # BNCS protocol-select byte
        s.sendall(payload)
        time.sleep(0.05)
        try:
            s.recv(256)
        except OSError:
            pass
        s.close()
    except OSError:
        pass


def _raw_wol(host, port, payload):
    try:
        s = socket.create_connection((host, port), timeout=3.0)
        s.settimeout(3.0)
        s.sendall(payload)
        time.sleep(0.05)
        try:
            s.recv(256)
        except OSError:
            pass
        s.close()
    except OSError:
        pass


def _bncs_battery(host, port):
    H = 0xFF
    cases = [
        b"\xff",                                   # truncated header
        b"\xff\x50",                               # half header
        struct.pack("<BBH", H, 0x50, 1),           # length < header size
        struct.pack("<BBH", H, 0x50, 0),           # length 0
        struct.pack("<BBH", H, 0x3a, 4),           # LOGONRESPONSE2, empty body
        struct.pack("<BBH", H, 0x0c, 4),           # JOINCHANNEL, empty body
        struct.pack("<BBH", H, 0xee, 4),           # unknown SID
        struct.pack("<BBH", H, 0x50, 0xffff) + b"AB",   # lying huge length
        struct.pack("<BBH", H, 0x09, 4),           # GETADVLISTEX, empty body
        # READUSERDATA with count fields = 0xFFFFFFFF (allocation/loop overflow bait)
        struct.pack("<BBH", H, 0x26, 16) + struct.pack("<III", 0xffffffff, 0xffffffff, 0),
        struct.pack("<BBH", H, 0x26, 4 + 4) + b"\xff\xff\xff\xff",  # truncated counts
        b"\xff\x50\x10\x00" + b"\xff" * 4096,      # oversized vs claimed
        b"garbagegarbage\x00\x01\x02",             # non-protocol junk
    ]
    for c in cases:
        _raw_bncs(host, port, c)


def _wol_battery(host, port):
    cases = [
        b"A" * 70000,                  # over-long, no CRLF
        b"NICK\r\n",                   # missing param
        b"USER\r\n",
        b"JOIN\r\n",
        b"PRIVMSG\r\n",                # pre-login, missing params
        b"LIST\r\n",                   # pre-login
        b"FLOOBLE x y z\r\n",          # unknown command
        b"NICK " + b"x" * 100000 + b"\r\n",   # huge param
        b"\x00\x01\x02\xff binary\r\n",        # binary/NUL
        b"VERCHK notanumber huge\r\n",
        b"\r\n\r\n\r\n",               # blank lines
    ]
    for c in cases:
        _raw_wol(host, port, c)


def _bncs_alive(host, port):
    try:
        c = bc.BncsClient(host, port)
        tok = bc.auth_handshake(c)
        c.close()
        return tok is not None
    except Exception:
        return False


def _wol_alive(host, wolport):
    try:
        c = wc.wol_session(host, wolport, "liveprobe", "pw")
        ok = c is not None
        if c:
            c.close()
        return ok
    except Exception:
        return False


def liveness(host, port, wolport):
    _bncs_battery(host, port)
    _wol_battery(host, wolport)
    time.sleep(0.3)
    return {"bncs_alive": _bncs_alive(host, port),
            "wol_alive": _wol_alive(host, wolport)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6394)
    ap.add_argument("--v3-port", type=int, default=6494)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = liveness("127.0.0.1", args.orig_port, orig.wolv1_port)
        n = liveness("127.0.0.1", args.v3_port, v3.wol_port)
        print(f"{'check':<14}{'oracle':<10}{'v3':<10}match")
        print("-" * 44)
        rows = [("bncs_alive", o["bncs_alive"], n["bncs_alive"]),
                ("wol_alive", o["wol_alive"], n["wol_alive"])]
        all_ok = True
        for name, ov, nv in rows:
            same = ov == nv
            all_ok &= same
            print(f"{name:<14}{str(ov):<10}{str(nv):<10}{'OK' if same else 'DIFF'}")
        print()
        # Decisive: v3 must survive the battery (both protocols still serve).
        success = all_ok and n["bncs_alive"] and n["wol_alive"]
        if success:
            print("v3 survives the malformed-input battery (no crash/hang).")
            return 0
        print("FAIL: server did not survive malformed input.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
