#!/usr/bin/env python3
"""Differential test: WarCraft III SRP-3 (NLS) password change (0x55/0x56).

Drives, against BOTH the original pvpgn-server and v3:

    create_account_w3(user, OLD)          -> account + SRP-3 creds
    passchange_w3(user, OLD, NEW)         -> 0x55 challenge (prove OLD) + 0x56
                                             proof (store NEW salt+verifier)
    login_w3(user, NEW)                   -> must now succeed with the NEW pass

and diffs the change result plus the post-change re-login. A match means v3
implements the real NLS password-change protocol: it proves the old password via
SRP-3 M1, stores the new salt+verifier, and the new password then authenticates.

Regression guard for the NLS passchange implementation.

Run: python3 tests/diff/diff_w3_passchange.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def scenario(host, port, user="w3pc"):
    out = {}
    c = bc.BncsClient(host, port)
    bc.auth_handshake(c, product=b"WAR3", client_token=0xDEADBEEF)
    out["create"] = bc.create_account_w3(c, user, "oldpass")
    pc = bc.passchange_w3(c, user, "oldpass", "newpass")
    out["change_msg"] = pc["change_msg"]
    out["change_proof"] = pc["proof_response"]
    out["change_m2"] = pc["m2_matches"]
    c.close()

    # Fresh connection: the NEW password must now log in; the OLD must not.
    c2 = bc.BncsClient(host, port)
    bc.auth_handshake(c2, product=b"WAR3", client_token=0xDEADBEEF)
    rn = bc.login_w3(c2, user, "newpass")
    out["new_login_proof"] = rn["proof_response"]   # 0 or 0x0E == success
    out["new_login_m2"] = rn["m2_matches"]
    c2.close()

    c3 = bc.BncsClient(host, port)
    bc.auth_handshake(c3, product=b"WAR3", client_token=0xDEADBEEF)
    ro = bc.login_w3(c3, user, "oldpass")
    out["old_login_m2"] = ro["m2_matches"]          # must be False after change
    c3.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6376)
    ap.add_argument("--v3-port", type=int, default=6476)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        keys = ["create", "change_msg", "change_proof", "change_m2",
                "new_login_proof", "new_login_m2", "old_login_m2"]
        print(f"{'field':<18}{'original':<12}{'v3':<12}match")
        print("-" * 50)
        all_ok = True
        for k in keys:
            same = o.get(k) == n.get(k)
            all_ok &= same
            print(f"{k:<18}{str(o.get(k)):<12}{str(n.get(k)):<12}"
                  f"{'OK' if same else 'DIFF'}")
        success = (all_ok and o.get("change_m2") is True
                   and o.get("change_proof") == 0
                   and o.get("new_login_m2") is True
                   and o.get("old_login_m2") is False)
        print()
        if success:
            print("WarCraft III SRP-3 password change matches the oracle "
                  "(prove old, store new, new password logs in).")
            return 0
        print("FAIL: divergence in WarCraft III SRP-3 password change.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
