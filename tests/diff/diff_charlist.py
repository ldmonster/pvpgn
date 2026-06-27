#!/usr/bin/env python3
"""Differential test: SID_CHARLIST (0x37) reply.

A logged-in Diablo II client may request its closed-realm character list with
CLIENT_UNKNOWN_37 / SID_CHARLIST (0x37). The original server's
_client_charlistreq answers UNCONDITIONALLY with SERVER_UNKNOWN_37 (0x37):

  * unknown1   u32 == SERVER_UNKNOWN_37_UNKNOWN1 (0)
  * max_chars  u32 == SERVER_UNKNOWN_37_UNKNOWN2 (8)
  * count      u32 == number of closed characters
  * `count` character records follow.

For any plain account the closed-character list is NULL, so count == 0 and the
body is exactly 12 bytes: 00 00 00 00 08 00 00 00 00 00 00 00. This is
config/backend-independent. v3 previously accepted 0x37 silently and sent
NOTHING, leaving a D2 client waiting on its character list.

What is compared (decisive, config-independent observables):
  1. Both servers answer 0x37 with a 0x37.
  2. Both reply bodies are >= 12 bytes (unknown1 + max_chars + count present).
  3. unknown1 == 0 on both.
  4. The character count matches between the two servers (0 on a plain account).

Run: python3 tests/diff/diff_charlist.py
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_CHARLIST = 0x37


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


def _parse_charreply(body):
    """Return (ok, unknown1, count). ok iff at least the 12-byte head present."""
    if body is None or len(body) < 12:
        return False, None, None
    unknown1, _max_chars, count = struct.unpack_from("<III", body, 0)
    return True, unknown1, count


def scenario(host, port):
    out = {}
    c, _ = bc.full_login(host, port, "charlie", "charliepass")

    # opencount u32 (0 in practice).
    c.send(SID_CHARLIST, struct.pack("<I", 0))
    body = _recv_match(c, SID_CHARLIST)
    ok, u1, cnt = _parse_charreply(body)
    out["reply"] = body is not None
    out["head_ok"] = ok
    out["unknown1"] = u1
    out["count"] = cnt

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
    ap.add_argument("--orig-port", type=int, default=8840)
    ap.add_argument("--v3-port", type=int, default=8860)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<14}{'original':<14}{'v3':<14}match")
        print("-" * 52)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<14}{str(o[k]):<14}{str(n[k]):<14}{'OK' if same else 'DIFF'}")
        print()

        # Sanity: the oracle must answer with a well-formed head (unknown1 == 0).
        oracle_ok = (o.get("reply") and o.get("head_ok")
                     and o.get("unknown1") == 0)
        if not oracle_ok:
            print("FAIL: oracle did not behave as expected — probe/setup broken.")
            return 2
        if all_ok:
            print("SID_CHARLIST reply (0x37) matches the oracle.")
            return 0
        print("FAIL: v3 diverges from the oracle on SID_CHARLIST.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
