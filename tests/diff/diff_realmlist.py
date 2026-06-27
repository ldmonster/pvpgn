#!/usr/bin/env python3
"""Differential test: SID_REALMLISTREQ replies (0x40 110-era + 0x34 legacy).

A logged-in client may query the Diablo II realm list. The original server
answers BOTH variants unconditionally, even with no realms configured:

  * CLIENT_REALMLISTREQ_110 (0x40, header-only body) -> SERVER_REALMLISTREPLY_110
    (0x40): reserved u32 (== 0) + u32 realm count, followed by `count` records.
  * CLIENT_REALMLISTREQ     (0x34, two u32 cookies)  -> SERVER_REALMLISTREPLY
    (0x34): reserved u32 (== 0) + u32 realm count, followed by `count` records.

(_client_realmlistreq110 / _client_realmlistreq, handle_bnet.cpp — registered
ONLY in the logged-in handler table, so the request must follow login.)

The default config has no active realms, so count == 0 and the body is exactly
8 bytes: 00 00 00 00 00 00 00 00.  v3 previously accepted these silently and
sent NOTHING, leaving a D2 client waiting on the realm list.

What is compared (decisive, config-independent observables):
  1. Both servers answer 0x40 with a 0x40 and 0x34 with a 0x34.
  2. Both reply bodies are >= 8 bytes (reserved u32 + count u32 present).
  3. The reserved u32 (unknown1) is 0 on both.
  4. The realm count matches between the two servers (0 in the default config).

Run: python3 tests/diff/diff_realmlist.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_REALMLISTREQ_110 = 0x40
SID_REALMLISTREQ = 0x34


def _recv_match(client, want_sid, tries=20):
    for _ in range(tries):
        r = client.recv()
        if r is None:
            return None
        sid, body = r
        if sid == bc.SID_PING:
            client.send(bc.SID_PING, body[:4])
            continue
        if sid == want_sid:
            return body
    return None


def _parse_realmreply(body):
    """Return (ok, unknown1, count). ok iff at least the 8-byte head is present."""
    if body is None or len(body) < 8:
        return False, None, None
    unknown1, count = struct.unpack_from("<II", body, 0)
    return True, unknown1, count


def scenario(host, port):
    out = {}
    c, _ = bc.full_login(host, port, "alice", "alicepass")

    # 0x40 (1.10-era): header-only request.
    c.send(SID_REALMLISTREQ_110, b"")
    body110 = _recv_match(c, SID_REALMLISTREQ_110)
    ok110, u1_110, cnt110 = _parse_realmreply(body110)
    out["reply_110"] = body110 is not None
    out["head_ok_110"] = ok110
    out["unknown1_110"] = u1_110
    out["count_110"] = cnt110

    # 0x34 (legacy): two u32 cookies (zero in practice).
    c.send(SID_REALMLISTREQ, struct.pack("<II", 0, 0))
    body34 = _recv_match(c, SID_REALMLISTREQ)
    ok34, u1_34, cnt34 = _parse_realmreply(body34)
    out["reply_legacy"] = body34 is not None
    out["head_ok_legacy"] = ok34
    out["unknown1_legacy"] = u1_34
    out["count_legacy"] = cnt34

    c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=8740)
    ap.add_argument("--v3-port", type=int, default=8760)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<18}{'original':<14}{'v3':<14}match")
        print("-" * 56)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<18}{str(o[k]):<14}{str(n[k]):<14}{'OK' if same else 'DIFF'}")
        print()

        # Sanity: the oracle must answer both variants with a well-formed head
        # whose reserved u32 is 0 (guards the probe itself).
        oracle_ok = (o.get("reply_110") and o.get("head_ok_110")
                     and o.get("unknown1_110") == 0
                     and o.get("reply_legacy") and o.get("head_ok_legacy")
                     and o.get("unknown1_legacy") == 0)
        if not oracle_ok:
            print("FAIL: oracle did not behave as expected — probe/setup broken.")
            return 2
        if all_ok:
            print("SID_REALMLISTREQ replies (0x40 + 0x34) match the oracle.")
            return 0
        print("FAIL: v3 diverges from the oracle on SID_REALMLISTREQ.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
