#!/usr/bin/env python3
"""Differential test: BNCS SID_PROFILE (0x35) profile view ("/finger").

Drives, against BOTH the original pvpgn-server and v3:

    A: OLS login + enter chat
    A: WRITEUSERDATA profile\\description="hello world"  profile\\location="NY"
    A: PROFILE A  ->  {fail:0, description:"hello world", location:"NY", clan_tag:0}

and diffs the profile reply. A match means v3 serves the SID_PROFILE view from the
account's profile attributes the way the original does (account_get_desc /
account_get_loc). Unlike STATSREPLY this reply has no timestamps, so it is a
byte-exact comparison.

Run: python3 tests/diff/diff_profile.py --v3-bnetd <path>
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

USER = "finguser"
DESC = "hello world"
LOC = "NY"


def scenario(host, port):
    try:
        c, _uniq = bc.full_login(host, port, USER, "secretpass")
    except Exception as e:  # noqa: BLE001
        return {"profile": None, "err": str(e)}
    try:
        bc.write_userdata(c, USER, {
            "profile\\description": DESC,
            "profile\\location": LOC,
        })
        prof = bc.request_profile(c, USER)
        return {"profile": prof, "err": None}
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
    ap.add_argument("--orig-port", type=int, default=6442)
    ap.add_argument("--v3-port", type=int, default=6542)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<10}{'original':<34}{'v3':<34}match")
        print("-" * 84)
        ov, nv = str(o["profile"]), str(n["profile"])
        m = ov == nv
        print(f"{'profile':<10}{ov:<34}{nv:<34}{'OK' if m else 'DIFF'}")
        print()

        want = {"fail": 0, "description": DESC, "location": LOC, "clan_tag": 0}
        if m and o["profile"] == want:
            print("BNCS SID_PROFILE matches the oracle (desc/location served).")
            return 0
        print(f"FAIL: BNCS SID_PROFILE divergence (want {want}, "
              f"orig={o}, v3={n}).")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
