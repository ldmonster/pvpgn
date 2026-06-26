#!/usr/bin/env python3
"""Differential test: WOL CHANCHK (channel check) + HOST (host relay).

Drives, against BOTH the original pvpgn-server and v3 (WOL listener):

  CHANCHK:
    client A: JOIN #chkroom
    client A: CHANCHK #chkroom   -> ":<server> CHANCHK #chkroom"  (exists)
    client A: CHANCHK #ghost     -> 403 ERR_NOSUCHCHANNEL          (absent)

  HOST:
    client A: HOST hostb :hello  -> client B receives ":..!.. HOST : hello"
    client A: HOST ghost :hello  -> client A receives 401 ERR_NOSUCHNICK

and diffs: CHANCHK status for an existing vs absent channel, and HOST delivery to
an online target vs 401 for an unknown one. A match means v3 reproduces the WOL
CHANCHK reply and HOST relay the way the original does.

Run: python3 tests/diff/diff_wol_chanchk_host.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

ROOM = "chkroom"


def scenario(host, port, sku=1000):
    b = wc.wol_session(host, port, "hostb", "secretpass", sku=sku)
    a = wc.wol_session(host, port, "hosta", "secretpass", sku=sku)
    if a is None or b is None:
        for c in (a, b):
            if c:
                c.close()
        return {k: None for k in
                ("chk_exists", "chk_absent", "host_delivered", "host_401")}
    try:
        wc.wol_join(a, f"#{ROOM}")
        chk_exists = wc.wol_chanchk(a, f"#{ROOM}")
        chk_absent = wc.wol_chanchk(a, "#ghostchan_nope")

        a.send_line("HOST hostb :hello")
        host_delivered = wc.wol_read_verb(b, "HOST") is not None
        a.send_line("HOST ghostnick_nope :hello")
        host_401 = wc.wol_read_numeric(a, 401) is not None

        return {
            "chk_exists": chk_exists,
            "chk_absent": chk_absent,
            "host_delivered": host_delivered,
            "host_401": host_401,
        }
    finally:
        a.close()
        b.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6422)
    ap.add_argument("--v3-port", type=int, default=6522)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port, sku=1000)
        n = scenario("127.0.0.1", v3.wol_port, sku=1000)

        print(f"{'field':<16}{'original':<14}{'v3':<14}match")
        print("-" * 54)
        ok = True
        for k in ("chk_exists", "chk_absent", "host_delivered", "host_401"):
            ov, nv = str(o[k]), str(n[k])
            m = ov == nv
            ok = ok and m
            print(f"{k:<16}{ov:<14}{nv:<14}{'OK' if m else 'DIFF'}")
        print()

        if (ok and o["chk_exists"] == "chanchk" and o["chk_absent"] == "403"
                and o["host_delivered"] is True and o["host_401"] is True):
            print("WOL CHANCHK + HOST match the oracle.")
            return 0
        print("FAIL: WOL CHANCHK/HOST divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
