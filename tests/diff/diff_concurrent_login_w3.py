#!/usr/bin/env python3
"""Differential test: concurrent same-account W3/NLS login (kick-old-login).

For OLS the second login's result code shows the kick; for W3 the proof reply is
"success" either way, so the observable is whether the OLD connection gets
dropped. Drives, against BOTH servers:

    conn1: WAR3 auth + create + login A     (stay connected)
    conn2: WAR3 auth + login A
    -> is conn1 dropped (kicked)?  (kick_old_login=true default)

and diffs whether the old connection is kicked. A divergence means v3's W3 login
leaves the old session in place (and the new one unregistered) instead of
kicking.

Run: python3 tests/diff/diff_concurrent_login_w3.py --v3-bnetd <path>
"""
import argparse
import os
import socket
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

USER = "w3concur"


def w3_login_conn(host, port, create=False):
    c = bc.BncsClient(host, port)
    bc.auth_handshake(c, product=b"WAR3", client_token=0xDEADBEEF)
    if create:
        bc.create_account_w3(c, USER, "secretpass")
    res = bc.login_w3(c, USER, "secretpass")
    return c, res


def is_kicked(client, wait=2.0):
    """True iff the server has closed the connection (EOF), reading past any
    final packets it may send before the close."""
    client.sock.settimeout(wait)
    try:
        while True:
            data = client.sock.recv(4096)
            if data == b"":
                return True
    except socket.timeout:
        return False
    except OSError:
        return True


def scenario(host, port):
    c1, r1 = w3_login_conn(host, port, create=True)
    try:
        c2, r2 = w3_login_conn(host, port)
        try:
            return {
                # m2_matches == mutual-auth success (the proof reply may be
                # 0x0E RESPONSE_EMAIL, still a successful login, so don't key on
                # proof_response == 0).
                "first_ok": r1.get("m2_matches") is True,
                "second_ok": r2.get("m2_matches") is True,
                "old_kicked": is_kicked(c1),
            }
        finally:
            c2.close()
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
    ap.add_argument("--orig-port", type=int, default=6450)
    ap.add_argument("--v3-port", type=int, default=6550)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<12}{'original':<12}{'v3':<12}match")
        print("-" * 48)
        for k in ("first_ok", "second_ok", "old_kicked"):
            ov, nv = str(o[k]), str(n[k])
            print(f"{k:<12}{ov:<12}{nv:<12}{'OK' if ov == nv else 'DIFF'}")
        print()
        if o == n and o["old_kicked"] is True:
            print("W3 concurrent login matches the oracle (old connection kicked).")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n}")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
