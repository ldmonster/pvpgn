#!/usr/bin/env python3
"""Differential test: SID_CHANGECLIENT (0x5C) invalid switch drops the socket.

The original (`_client_changeclient`, handle_bnet.cpp, registered pre-login in
bnet_htable_con) only permits a client switch when the connection's current tag
is WAR3XP ("W3XP") AND the requested new tag is WARCRAFT3 ("WAR3"). On any other
combination it logs "invalid attempt to change client", calls
conn_set_state(conn_state_destroy) and returns -1 — i.e. it FINs the socket. On
the valid path it just swaps the clienttag and sends no reply.

v3's BnetFsm::on(ChangeClient) was an unconditional no-op that accepted every
CHANGECLIENT and kept the connection alive. This guards the invalid-path destroy.

Decisive observable (deterministic in this harness): after an OLS auth_handshake
with product=SEXP (conn clienttag SEXP, != W3XP), sending SID_CHANGECLIENT b"WAR3"
must make BOTH servers drop the socket (recv -> b"" == "closed"). A control
connection that sends no CHANGECLIENT stays alive on BOTH.

(The valid W3XP->WAR3 path is not reachable in this harness — the oracle's stored
clienttag after auth_handshake is not actually W3XP here — so we assert only the
clean invalid-path destroy.)

Run: python3 tests/diff/diff_changeclient.py
"""
import argparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

SID_CHANGECLIENT = 0x5C


def liveness(sock):
    sock.settimeout(2.5)
    try:
        return "closed" if sock.recv(4096) == b"" else "alive"
    except socket.timeout:
        return "alive"
    except (ConnectionError, OSError):
        return "closed"


def probe(host, port, send_cc):
    c = bc.BncsClient(host, port)
    _, res, _ = bc.auth_handshake(c, product=b"SEXP")
    if send_cc:
        c.send(SID_CHANGECLIENT, b"WAR3")
    time.sleep(0.3)
    live = liveness(c.sock)
    c.close()
    return {"auth_res": res, "live": live}


def scenario(host, port):
    return {
        "control": probe(host, port, send_cc=False),
        "changeclient": probe(host, port, send_cc=True),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=12640)
    ap.add_argument("--v3-port", type=int, default=12646)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)

        print(f"{'field':<28}{'original':<12}{'v3':<12}match")
        print("-" * 64)
        all_ok = True
        for grp in ("control", "changeclient"):
            for k in ("auth_res", "live"):
                ov, nv = str(o[grp][k]), str(n[grp][k])
                same = ov == nv
                all_ok &= same
                print(f"{grp + '.' + k:<28}{ov:<12}{nv:<12}"
                      f"{'OK' if same else 'DIFF'}")
        print()

        # The oracle must demonstrate the invalid-path destroy for the test to
        # be meaningful, and v3 must match it; control must stay alive on both.
        meaningful = (o["changeclient"]["live"] == "closed"
                      and o["control"]["live"] == "alive")
        if all_ok and meaningful:
            print("Invalid SID_CHANGECLIENT drops the socket (matches oracle).")
            return 0
        print(f"DIVERGENCE: orig={o} v3={n}")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
