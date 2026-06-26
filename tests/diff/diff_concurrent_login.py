#!/usr/bin/env python3
"""Differential test: concurrent same-account login (kick-old-login behavior).

The original defaults to kick_old_login=true: when an account logs in while a
previous connection is still online, the OLD connection is dropped and the NEW
login succeeds. Drives, against BOTH servers:

    conn1: login A           (stay connected)
    conn2: login A           -> expect success (old kicked), per the default

and diffs the second login's result code. A divergence means v3's single-session
policy rejects the new login instead of kicking the old one.

Run: python3 tests/diff/diff_concurrent_login.py --v3-bnetd <path>
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

USER = "concuruser"


def login_conn(host, port, create=False):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    if create:
        bc.create_account_ols(c, USER, "pw")
    rc = bc.login_ols(c, USER, "pw", ctok, stok)
    return c, rc


def scenario(host, port):
    c1, rc1 = login_conn(host, port, create=True)
    try:
        c2, rc2 = login_conn(host, port)
        c2.close()
        return {"first": rc1, "second": rc2}
    finally:
        c1.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6444)
    ap.add_argument("--v3-port", type=int, default=6546)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<10}{'original':<14}{'v3':<14}match")
        print("-" * 46)
        for k in ("first", "second"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<10}{ov:<14}{nv:<14}{'OK' if ov == nv else 'DIFF'}")
        print()
        if o == n:
            print("Concurrent login matches the oracle.")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n} "
              "(oracle default kick_old_login=true allows the new login).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
