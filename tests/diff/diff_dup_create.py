#!/usr/bin/env python3
"""Differential test: duplicate OLS account creation.

    conn: create A   -> result1 (OK)
    conn: create A   -> result2 (should be rejected: name already exists)

Diffs the second create's result code against the oracle.

Run: python3 tests/diff/diff_dup_create.py --v3-bnetd <path>
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

USER = "dupcreate"


def scenario(host, port):
    c = bc.BncsClient(host, port)
    try:
        bc.auth_handshake(c, client_token=0xDEADBEEF)
        r1 = bc.create_account_ols(c, USER, "pw")
        r2 = bc.create_account_ols(c, USER, "pw")
        return {"first": r1, "second": r2}
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
    ap.add_argument("--orig-port", type=int, default=6452)
    ap.add_argument("--v3-port", type=int, default=6552)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"{'field':<10}{'original':<12}{'v3':<12}match")
        print("-" * 46)
        for k in ("first", "second"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<10}{ov:<12}{nv:<12}{'OK' if ov == nv else 'DIFF'}")
        print()
        if o == n:
            print("Duplicate account creation matches the oracle.")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n}")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
