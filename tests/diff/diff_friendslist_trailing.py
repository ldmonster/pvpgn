#!/usr/bin/env python3
"""Differential test: SID_FRIENDSLIST (0x65) tolerates trailing body bytes.

SID_FRIENDSLIST carries no payload. The original server's _client_friendslistreq
only enforces a *minimum* size (`packet_get_size(packet) <
sizeof(t_client_friendslistreq)`, i.e. the bare header), so it accepts — and
ignores — any trailing bytes a client appends, still replying with the friends
list. v3's decoder originally demanded an EXACTLY empty body
(`check_empty_body`), so a padded request was silently dropped (no reply) where
the oracle answered. This pins the leniency: empty, small-trailing, and
large-trailing FRIENDSLISTREQ must all produce a SERVER_FRIENDSLIST reply on
both servers.

Run: python3 tests/diff/diff_friendslist_trailing.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_FRIENDSLIST = 0x65


def _collect(c, timeout=1.5, max_packets=12):
    c.sock.settimeout(timeout)
    out = []
    for _ in range(max_packets):
        try:
            r = c.recv()
        except Exception:
            break
        if r is None:
            break
        sid, body = r
        if sid == bc.SID_PING:
            c.send(bc.SID_PING, body[:4])
            continue
        out.append((sid, len(body)))
    return out


def scenario(host, port):
    out = {}
    for name, body in [("empty", b""),
                       ("trailing2", b"\xff\xff"),
                       ("trailing64", b"X" * 64)]:
        c, _ = bc.full_login(host, port, "flprobe", "pw")
        c.send(SID_FRIENDSLIST, body)
        replies = _collect(c)
        # Did we get at least one FRIENDSLIST reply (0x65)?
        out[name] = any(sid == SID_FRIENDSLIST for sid, _ in replies)
        c.close()
        time.sleep(0.05)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6396)
    ap.add_argument("--v3-port", type=int, default=6496)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"{'case':<14}{'oracle':<10}{'v3':<10}match")
        print("-" * 44)
        all_ok = True
        for k in o:
            same = o[k] == n[k]
            all_ok &= same
            print(f"{k:<14}{str(o[k]):<10}{str(n[k]):<10}{'OK' if same else 'DIFF'}")
        print()
        # v3 must reply in every case (matching the oracle's leniency).
        success = all_ok and all(n.values())
        print("MATCH" if success else "MISMATCH")
        return 0 if success else 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
