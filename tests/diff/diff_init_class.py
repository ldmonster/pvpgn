#!/usr/bin/env python3
"""Differential test: the BNCS/BNFTP shared-port connection-class selector byte.

The very first byte a client sends on the shared BNCS+BNFTP listener is the
connection-class selector (`CLIENT_INITCONN_CLASS_*` in init_protocol.h):

    0x01 = BNET   0x02 = FILE/BNFTP   0x03 = BOT   0x04 = ENC
    0x0d = TELNET 0x65 = D2CS_BNETD   0x98 = LOCALMACHINE

The original's handle_init_packet (src/bnetd/handle_init.cpp) returns -1 ->
destroys the connection for ENC (0x04), LOCALMACHINE (0x98), D2CS_BNETD from a
non-realm IP (0x65), and every unknown byte (default case). v3's live dispatch
used to special-case only 0xFF/0x01 as BNet and route EVERY other first byte
into BnftpFsm, which simply waits for more bytes — so unknown/unsupported init
classes were silently held OPEN (a remote half-open-connection exhaustion
surface). This guards that v3 now CLOSES on the unsupported/unknown classes,
matching the oracle, while leaving the accepted classes OPEN.

Decisive observable (probe, fresh connection per byte): send a single init byte,
wait, then recv() with a timeout:
    EOF (b'')  => CLOSED
    timeout    => OPEN (still alive)

Expectation (both servers):
    0x04/0x05/0x65/0x98/0xAB/0x00 -> CLOSED on both
    0x01/0x02                     -> OPEN on both (real BNet / BNFTP streams)

(0x03/0x0d -> BOT/Telnet: the oracle keeps the socket OPEN and emits a prompt;
v3 keeps it OPEN without a prompt. Both OPEN, so the close-state matches; the
prompt text is bot/telnet-specific and out of scope here. 0xFF is treated by v3
as a BNet stream from a client that already stripped the init byte — an
intentional v3 accommodation — so it is excluded from this guard.)

Run: python3 tests/diff/diff_init_class.py
"""
import argparse
import os
import socket
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402

# init byte -> expected connection state after sending it alone.
CLOSED = "CLOSED"
OPEN = "OPEN"
EXPECT = {
    0x04: CLOSED,  # ENC          -> reject
    0x05: CLOSED,  # unknown      -> reject
    0x65: CLOSED,  # D2CS_BNETD   -> non-realm IP reject
    0x98: CLOSED,  # LOCALMACHINE -> reject
    0xAB: CLOSED,  # unknown      -> reject
    0x00: CLOSED,  # unknown      -> reject
    0x01: OPEN,    # BNET         -> accepted (login stream)
    0x02: OPEN,    # FILE/BNFTP   -> accepted (file request)
}


def probe(host, port, init_byte, settle=0.4, timeout=2.0):
    """Send one init byte; classify the connection as CLOSED or OPEN."""
    s = socket.create_connection((host, port), timeout=4.0)
    s.settimeout(timeout)
    try:
        s.sendall(bytes([init_byte]))
        time.sleep(settle)
        try:
            data = s.recv(4096)
            # EOF means the peer closed the connection.
            return CLOSED if data == b"" else OPEN
        except socket.timeout:
            return OPEN
        except (ConnectionResetError, ConnectionAbortedError, OSError):
            return CLOSED
    finally:
        try:
            s.close()
        except OSError:
            pass


def collect(host, port):
    return {b: probe(host, port, b) for b in sorted(EXPECT)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=12320)
    ap.add_argument("--v3-port", type=int, default=12326)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        time.sleep(1.0)
        o = collect("127.0.0.1", args.orig_port)
        n = collect("127.0.0.1", args.v3_port)

        print(f"{'byte':<8}{'expect':<10}{'original':<10}{'v3':<10}match")
        print("-" * 48)
        all_ok = True
        for b in sorted(EXPECT):
            exp, ov, nv = EXPECT[b], o[b], n[b]
            # Oracle must demonstrate the expected behavior, and v3 must
            # match the oracle.
            same = (ov == exp) and (nv == ov)
            all_ok &= same
            print(f"0x{b:02x}    {exp:<10}{ov:<10}{nv:<10}"
                  f"{'OK' if same else 'DIFF'}")
        print()
        if all_ok:
            print("Init-class dispatch matches oracle "
                  "(rejects unsupported/unknown, accepts BNet/BNFTP).")
            return 0
        print(f"DIVERGENCE: expect={EXPECT} orig={o} v3={n}")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
