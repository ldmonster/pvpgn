#!/usr/bin/env python3
"""Differential test: MF_PLUG (no-UDP plug) on the FIRST channel join.

The original creates every bnet connection with MF_PLUG (0x10) set
(connection.cpp:383) and clears it on the FIRST channel join via
channel_set_userflags (handle_bnet.cpp:3704). What OTHER channel members observe
for a freshly-connected, normal (non-op) user is therefore:

    first join:  EID_JOIN(flags=0x10) then TWO EID_USERFLAGS(flags=0)
                 (channel_set_userflags double-broadcasts: conn_set_flags() AND a
                  direct channel_update_userflags())
    later joins: EID_JOIN(flags=0) with NO trailing USERFLAGS (plug already shed)

v3 previously sent only EID_JOIN(flags=0) with no plug and no trailing USERFLAGS,
diverging from the oracle on the first-join icon state. This test drives both:
  - bob watches alice's FIRST join     -> expect [(JOIN,16),(UF,0),(UF,0)]
  - carol watches alice's SECOND join  -> expect [(JOIN,0)]
and diffs the (event_id, flags, username) stream each member observes.

Run: python3 tests/diff/diff_join_userflags.py
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402


def _login(host, port, user, pw):
    c = bc.BncsClient(host, port)
    ctok = 0xDEADBEEF
    stok, _, _ = bc.auth_handshake(c, client_token=ctok)
    bc.create_account_ols(c, user, pw)
    bc.login_ols(c, user, pw, ctok, stok)
    bc.enter_chat(c, user)
    return c


def _parse(body):
    eid, flags, _ping = struct.unpack_from("<III", body, 0)
    name = body[24:].split(b"\x00")[0].decode("latin-1", "replace")
    return (eid, flags, name)


def _drain(client, settle=0.5):
    time.sleep(settle)
    old = client.sock.gettimeout()
    client.sock.settimeout(0.4)
    out = []
    try:
        while True:
            r = client.recv()
            if r is None:
                break
            sid, body = r
            if sid == bc.SID_PING:
                client.send(bc.SID_PING, body[:4])
                continue
            if sid == bc.SID_CHATEVENT:
                out.append(_parse(body))
    except Exception:
        pass
    finally:
        client.sock.settimeout(old)
    return out


def scenario(host, port):
    bob = _login(host, port, "ufbob", "bpw")
    bc.join_channel(bob, "UfChanOne")
    alice = _login(host, port, "ufalice", "apw")
    bc.drain_chat(bob)
    bc.join_channel(alice, "UfChanOne")     # alice's FIRST join
    first = _drain(bob)

    carol = _login(host, port, "ufcarol", "cpw")
    bc.join_channel(carol, "UfChanTwo")
    bc.drain_chat(carol)
    bc.join_channel(alice, "UfChanTwo")     # alice's SECOND join
    second = _drain(carol)

    for x in (bob, alice, carol):
        x.close()
    return {"first_by_bob": first, "second_by_carol": second}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=11400)
    ap.add_argument("--v3-port", type=int, default=11412)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print("ORIG:", o)
        print("V3  :", n)
        if o == n:
            print("OK: MF_PLUG first-join userflags match the oracle.")
            return 0
        print("DIVERGENCE")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
