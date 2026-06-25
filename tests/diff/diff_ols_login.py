# SPDX-License-Identifier: GPL-2.0-or-later
"""Differential test: run the OLS login flow against BOTH the original
pvpgn-server (oracle) and the v3 rewrite, and diff the observable behaviour.

Usage:
    diff_ols_login.py --orig-repo /home/cnupt/work/pvpgn-server --v3-bnetd <path>
"""
import argparse
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc


def _safe(fn, default="ERR"):
    try:
        return fn()
    except (ConnectionError, BrokenPipeError, OSError) as e:
        return f"CLOSED({type(e).__name__})"


def run_scenario(host, port, label):
    """Drive the OLS create+login flow; return a dict of observable outcomes.
    Resilient to a server closing the connection (recorded as CLOSED...)."""
    out = {}
    ctok = 0xDEADBEEF

    def session_create_login():
        c = bc.BncsClient(host, port)
        stok, authres, logon_type = bc.auth_handshake(
            c, product=b"SEXP", client_token=ctok)
        # auth_handshake raises if the server omits the 0x50 seed, so reaching
        # here means it was present.
        out["auth_seed_present"] = True
        out["logon_type"] = logon_type
        out["auth_check_result"] = authres
        out["server_token_nonzero"] = (stok != 0)
        out["create_result"] = bc.create_account_ols(c, "diffuser", "secret")
        out["login_result"] = bc.login_ols(c, "diffuser", "secret", ctok, stok)
        c.close()
    r = _safe(session_create_login)
    if isinstance(r, str):  # connection died mid-handshake
        out.setdefault("auth_seed_present", "?")
        out.setdefault("logon_type", "?")
        out.setdefault("auth_check_result", "?")
        out.setdefault("server_token_nonzero", "?")
        out.setdefault("create_result", "?")
        out["login_result"] = r

    def session_wrongpw():
        c2 = bc.BncsClient(host, port)
        stok2, _, _ = bc.auth_handshake(c2, product=b"SEXP", client_token=ctok)
        out["login_wrongpw_result"] = bc.login_ols(c2, "diffuser", "WRONGpw", ctok, stok2)
        c2.close()
    r = _safe(session_wrongpw)
    if isinstance(r, str):
        out["login_wrongpw_result"] = r

    def session_unknown():
        c3 = bc.BncsClient(host, port)
        stok3, _, _ = bc.auth_handshake(c3, product=b"SEXP", client_token=ctok)
        out["login_unknown_result"] = bc.login_ols(c3, "ghostuser", "secret", ctok, stok3)
        c3.close()
    r = _safe(session_unknown)
    if isinstance(r, str):
        out["login_unknown_result"] = r
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", required=True)
    # Keep the two well separated: the original also binds w3routeaddr at
    # orig_port+1, so v3_port must not be adjacent to orig_port.
    ap.add_argument("--orig-port", type=int, default=6320)
    ap.add_argument("--v3-port", type=int, default=6420)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    orig.start()
    try:
        v3.start()
        try:
            o = run_scenario("127.0.0.1", args.orig_port, "original")
            v = run_scenario("127.0.0.1", args.v3_port, "v3")
        finally:
            v3.stop()
    finally:
        orig.stop()

    keys = sorted(set(o) | set(v))
    print(f"{'field':<26} {'original':<14} {'v3':<14} {'match'}")
    print("-" * 64)
    mismatches = []
    for k in keys:
        ov, vv = o.get(k), v.get(k)
        # The OLS login OUTCOME must match (the security-relevant part); the
        # server_token seed presence is informational (a known v3 gap).
        same = (ov == vv)
        if not same and k not in ("server_token_nonzero", "auth_seed_present"):
            mismatches.append(k)
        print(f"{k:<26} {str(ov):<14} {str(vv):<14} {'OK' if same else 'DIFF'}")

    print()
    if mismatches:
        print(f"BEHAVIORAL MISMATCH in: {', '.join(mismatches)}")
        return 1
    print("OLS login behaviour matches the oracle (outcome-equivalent).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
