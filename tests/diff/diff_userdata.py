#!/usr/bin/env python3
"""Differential test: BNCS SID_WRITEUSERDATA / SID_READUSERDATA (profile fields).

Drives, against BOTH the original pvpgn-server and v3:

    A: OLS login + enter chat
    A: WRITEUSERDATA  profile\\sex=m  profile\\age=99  profile\\location=NY
    A: READUSERDATA   [profile\\sex, profile\\age, profile\\location, profile\\unset]
       -> ["m", "99", "NY", ""]

and diffs the values READUSERDATA returns (written fields echoed back, unset key
empty). A match means v3 stores and serves account profile attributes the way the
original does (account_set_strattr / account_get_strattr).

Run: python3 tests/diff/diff_userdata.py --v3-bnetd <path>
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

USER = "profuser"
WRITE = {
    "profile\\sex": "m",
    "profile\\age": "99",
    "profile\\location": "NY",
}
READ_KEYS = ["profile\\sex", "profile\\age", "profile\\location",
             "profile\\unsetkey"]
WANT = ["m", "99", "NY", ""]


def scenario(host, port):
    try:
        c, _uniq = bc.full_login(host, port, USER, "secretpass")
    except Exception as e:  # noqa: BLE001
        return {"values": None, "err": str(e)}
    try:
        bc.write_userdata(c, USER, WRITE)
        values = bc.read_userdata(c, USER, READ_KEYS)
        return {"values": values, "err": None}
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
    ap.add_argument("--orig-port", type=int, default=6440)
    ap.add_argument("--v3-port", type=int, default=6540)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<10}{'original':<28}{'v3':<28}match")
        print("-" * 72)
        ov, nv = str(o["values"]), str(n["values"])
        m = ov == nv
        print(f"{'values':<10}{ov:<28}{nv:<28}{'OK' if m else 'DIFF'}")
        print()

        if m and o["values"] == WANT:
            print("BNCS userdata matches the oracle (profile written + read back).")
            return 0
        print("FAIL: BNCS userdata divergence "
              f"(want {WANT}, orig={o}, v3={n}).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
