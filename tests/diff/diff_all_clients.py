#!/usr/bin/env python3
"""Differential driver for the full supported-client matrix (clients.py).

Boots BOTH the original pvpgn-server (the oracle) and v3, then for one
representative of every distinct protocol path drives the real login handshake
with the matching mock client and reports the outcome.

Coverage by family:
  * ols / nls  — driven against BOTH servers; the result must match (these paths
    are fully implemented in v3 and verified by the dedicated diff_*.py tests).
  * wol        — driven against the ORACLE (the "old implementation"), which is
    the contract these mocks target. v3 is probed best-effort on its fixed WOL
    port; v3 speaks a different WOL auth dialect (NICK/USER/PASS instead of the
    Westwood CVERS/APGAR flow), so a divergence there is EXPECTED and reported,
    not a test failure (see bug-hunt/findings/wol-chat-lobby.md).

Exit code: 0 iff every oracle login succeeds AND every ols/nls path matches v3.

Run: python3 tests/diff/diff_all_clients.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import clients as cat  # noqa: E402


def _user_for(spec, idx):
    # Unique per client so accounts never collide within a server's run.
    return f"c{idx}{spec.tag.decode().lower()}"


def run(oracle, v3, v3_wol_port):
    rows = []
    ok_all = True
    for idx, spec in enumerate(cat.representatives()):
        user = _user_for(spec, idx)
        pw = "secretpass1"
        row = {"name": spec.name, "tag": spec.tag.decode(),
               "family": spec.family, "note": ""}

        # --- oracle (the contract these mocks target) ---
        try:
            o = cat.login(spec, "127.0.0.1", oracle.port, user, pw,
                          wol_port=(oracle.wolv1_port if spec.wolv == 1
                                    else oracle.wolv2_port))
            row["oracle"] = o.get("ok", False)
        except Exception as e:  # noqa: BLE001 — surface harness/transport errors
            row["oracle"] = False
            row["note"] = f"oracle err: {e}"
        if not row["oracle"]:
            ok_all = False

        # --- v3 ---
        if spec.family in ("ols", "nls"):
            try:
                n = cat.login(spec, "127.0.0.1", v3.port, user, pw)
                row["v3"] = n.get("ok", False)
                row["match"] = (row["oracle"] == row["v3"])
                if not row["match"]:
                    ok_all = False
            except Exception as e:  # noqa: BLE001
                row["v3"] = False
                row["match"] = False
                row["note"] = f"v3 err: {e}"
                ok_all = False
        else:  # wol — best-effort probe against v3's fixed dialect/port
            try:
                n = cat.login(spec, "127.0.0.1", v3.port, user, pw,
                              wol_port=v3_wol_port)
                row["v3"] = n.get("ok", False)
            except Exception:  # noqa: BLE001 — v3 WOL may be unreachable
                row["v3"] = False
            # Divergence here is a known, documented gap — not a failure.
            row["match"] = (row["oracle"] == row["v3"])
            if not row["match"]:
                row["note"] = "known gap: v3 lacks WOL CVERS/APGAR auth"
        rows.append(row)
    return rows, ok_all


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6390)
    ap.add_argument("--v3-port", type=int, default=6490)
    ap.add_argument("--v3-wol-port", type=int, default=4000)
    args = ap.parse_args()

    print(f"Catalog: {len(cat.CATALOG)} products, {cat.total_versions()} "
          f"versions, {len(cat.representatives())} distinct protocol paths\n")

    oracle = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        oracle.start()
        v3.start()
        rows, ok_all = run(oracle, v3, args.v3_wol_port)
    finally:
        v3.stop()
        oracle.stop()

    print(f"{'client':34}{'fam':5}{'oracle':8}{'v3':6}{'match':7}note")
    print("-" * 78)
    for r in rows:
        print(f"{r['name'][:33]:34}{r['family']:5}"
              f"{str(r['oracle']):8}{str(r.get('v3','-')):6}"
              f"{str(r.get('match','-')):7}{r['note']}")

    n_ols_nls = sum(1 for r in rows if r["family"] in ("ols", "nls"))
    matched = sum(1 for r in rows
                  if r["family"] in ("ols", "nls") and r.get("match"))
    oracle_ok = sum(1 for r in rows if r["oracle"])
    print(f"\noracle logins OK: {oracle_ok}/{len(rows)}   "
          f"ols+nls match v3: {matched}/{n_ols_nls}")
    if ok_all:
        print("\nAll supported-client mocks log in against the oracle; "
              "OLS+NLS match v3.")
        return 0
    print("\nFAIL: an oracle login failed or an OLS/NLS path diverged from v3.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
