#!/usr/bin/env python3
"""Differential test: WarCraft III SRP-3 (NLS) login.

Drives a faithful WAR3 client handshake against BOTH the original pvpgn-server
and v3, and diffs the outcome:

    -> SID_AUTH_INFO (product WAR3)   => logon-type 2 seed
    -> SID_AUTH_CHECK                 => result 0
    -> SID_AUTH_ACCOUNTCREATE (0x52)  => store salt + SRP-3 verifier
    -> SID_AUTH_ACCOUNTLOGON (0x53)   => salt + server public key B
    -> SID_AUTH_ACCOUNTLOGONPROOF     => server proof M2

The SRP-3 crypto (bnet_srp3.py) is golden-verified bit-for-bit against the C++
implementation the original server uses, so a match here means v3 speaks the
real WarCraft III login protocol. The decisive check is `m2_matches`: the
client independently derives M2 and confirms the server returned the same — full
mutual authentication.

Regression guard for bug-hunt wave 21 (NLS differential).

Run: python3 tests/diff/diff_w3_login.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def w3_scenario(host, port):
    out = {}
    # New connection; faithful WAR3 auth handshake.
    c = bc.BncsClient(host, port)
    stok, authres, logon_type = bc.auth_handshake(
        c, product=b"WAR3", client_token=0xDEADBEEF)
    out["logon_type"] = logon_type           # expect 2 for WAR3
    out["auth_check"] = authres              # expect 0
    out["create"] = bc.create_account_w3(c, "w3diff", "secretpass")
    res = bc.login_w3(c, "w3diff", "secretpass")
    out["login_msg"] = res["login_msg"]          # 0 == account found
    out["proof_response"] = res["proof_response"]  # 0 == OK
    out["m2_matches"] = res["m2_matches"]        # mutual auth succeeded
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
    ap.add_argument("--orig-port", type=int, default=6360)
    ap.add_argument("--v3-port", type=int, default=6460)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = w3_scenario("127.0.0.1", args.orig_port)
        n = w3_scenario("127.0.0.1", args.v3_port)
        keys = ["logon_type", "auth_check", "create", "login_msg",
                "proof_response", "m2_matches"]
        print(f"{'field':<16}{'original':<14}{'v3':<14}match")
        print("-" * 52)
        all_ok = True
        for k in keys:
            same = o.get(k) == n.get(k)
            all_ok &= same
            print(f"{k:<16}{str(o.get(k)):<14}{str(n.get(k)):<14}"
                  f"{'OK' if same else 'DIFF'}")
        # The decisive outcome: both servers complete mutual SRP-3 auth. The
        # proof response is a successful-login code: 0x00 (OK) or 0x0E (login
        # OK, please register an e-mail) — the original returns the latter for
        # version id >= 0x0D accounts with no e-mail on file, and v3 mirrors it.
        ok_codes = (0x00, 0x0E)
        success = (all_ok and o.get("m2_matches") is True
                   and o.get("proof_response") in ok_codes)
        print()
        if success:
            print("WarCraft III SRP-3 login matches the oracle (mutual auth OK).")
            return 0
        print("FAIL: divergence in WarCraft III SRP-3 login.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
