#!/usr/bin/env python3
"""Differential test: Westwood Online (WOL) login.

Drives the faithful WOL/IRC handshake (CVERS → VERCHK → APGAR → NICK → USER →
welcome/MOTD) against BOTH the original pvpgn-server and v3, and diffs the
outcome across three scenarios:

  1. first login  — account auto-created, APGAR stored        => accepted (376)
  2. relogin OK   — same APGAR token                          => accepted (376)
  3. relogin BAD  — different APGAR token                     => rejected (378)

The original auto-creates the account on first login and compares the APGAR
token verbatim (a plain string compare). v3 mirrors this in wol_auth.cpp via the
WolAuthDeps (CreateAccount + IWolCredentialStore). A match here means v3 speaks
the real Westwood login protocol.

Regression guard for the v3 WOL-auth implementation.

Run: python3 tests/diff/diff_wol_login.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def wol_scenarios(host, port, sku=1000, user="woldiff"):
    out = {}
    # 1. first login (auto-create)
    r1 = wc.wol_login(host, port, user, "rightpass", sku=sku)
    out["first_ok"] = r1["ok"]
    out["first_code"] = r1["code"]
    # 2. relogin with the same password
    r2 = wc.wol_login(host, port, user, "rightpass", sku=sku)
    out["relogin_ok"] = r2["ok"]
    out["relogin_code"] = r2["code"]
    # 3. relogin with a WRONG password
    r3 = wc.wol_login(host, port, user, "wrongpass", sku=sku)
    out["bad_ok"] = r3["ok"]
    out["bad_code"] = r3["code"]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6380)
    ap.add_argument("--v3-port", type=int, default=6480)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        # SKU 1000 (Westwood Chat) is a wolv1 client on the original.
        o = wol_scenarios("127.0.0.1", orig.wolv1_port, sku=1000)
        n = wol_scenarios("127.0.0.1", v3.wol_port, sku=1000)
        keys = ["first_ok", "first_code", "relogin_ok", "relogin_code",
                "bad_ok", "bad_code"]
        print(f"{'field':<14}{'original':<12}{'v3':<12}match")
        print("-" * 46)
        all_ok = True
        for k in keys:
            same = o.get(k) == n.get(k)
            all_ok &= same
            print(f"{k:<14}{str(o.get(k)):<12}{str(n.get(k)):<12}"
                  f"{'OK' if same else 'DIFF'}")
        # Decisive: both auto-create+accept, both accept the correct re-login,
        # and both reject the wrong password.
        success = (all_ok and o.get("first_ok") is True
                   and o.get("relogin_ok") is True
                   and o.get("bad_ok") is False)
        print()
        if success:
            print("WOL login matches the oracle "
                  "(auto-create + APGAR verify).")
            return 0
        print("FAIL: divergence in WOL login.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
