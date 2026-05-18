#!/usr/bin/env python3
"""Minimal mock of the bnetd BNet surface, just enough to drive a
single v3 client tool (bnchat or bnstat) through the handshake.

Two ``--mode`` flavors:

* ``chat``  -- bnchat path: after the handshake expect
  CLIENT_LOGINREQ1 / CLIENT_PROGIDENT2 / CLIENT_JOINCHANNEL, then push
  one SERVER_MESSAGE (Talk) and close.
* ``stats`` -- bnstat path: after the handshake expect one
  CLIENT_STATSREQ and answer with a canned SERVER_STATSREPLY.

Shared wire helpers (recv_bnet/send_bnet/expect/do_handshake) live in
:mod:`fake_bnet_common`.
"""

from __future__ import annotations

import argparse
import struct
import sys

from fake_bnet_common import (
    do_handshake,
    expect,
    listen_one,
    send_bnet,
)


CHAT_MESSAGE_TEXT = "hello from fake bnetd"
CHAT_MESSAGE_FROM = "mockuser"

# bnstat with `-c D2DV` walks the four base profile keys
# (profile/sex, age, location, description) plus the D2DV record
# block.  We cycle these canned values over key_count.
STATS_VALUES = ["male", "42", "moon", "fake account"]

CLIENT_LOGINREQ1     = 0x29ff
SERVER_LOGINREPLY1   = 0x29ff
CLIENT_PROGIDENT2    = 0x0bff
SERVER_CHANNELLIST   = 0x0bff
CLIENT_JOINCHANNEL   = 0x0cff
SERVER_MESSAGE       = 0x0fff
CLIENT_STATSREQ      = 0x26ff
SERVER_STATSREPLY    = 0x26ff
STATSREQ_REQUEST_ID  = 0x02825278


def run_chat(conn) -> None:
    do_handshake(conn)
    expect(conn, CLIENT_LOGINREQ1, "CLIENT_LOGINREQ1")
    send_bnet(conn, SERVER_LOGINREPLY1, struct.pack("<I", 1))
    print("[fake-bnet] -> SERVER_LOGINREPLY1 (success)", flush=True)

    expect(conn, CLIENT_PROGIDENT2, "CLIENT_PROGIDENT2")
    send_bnet(conn, SERVER_CHANNELLIST, b"\x00")
    print("[fake-bnet] -> SERVER_CHANNELLIST (empty)", flush=True)

    expect(conn, CLIENT_JOINCHANNEL, "CLIENT_JOINCHANNEL")
    sm = struct.pack("<IIIIII", 5, 0, 0, 0, 0, 0)
    sm += CHAT_MESSAGE_FROM.encode("ascii") + b"\x00"
    sm += CHAT_MESSAGE_TEXT.encode("ascii") + b"\x00"
    send_bnet(conn, SERVER_MESSAGE, sm)
    print("[fake-bnet] -> SERVER_MESSAGE (Talk)", flush=True)


def run_stats(conn) -> None:
    do_handshake(conn)
    body = expect(conn, CLIENT_STATSREQ, "CLIENT_STATSREQ")
    name_count, key_count, _request_id = struct.unpack("<III", body[:12])
    if name_count != 1:
        raise RuntimeError(f"unexpected name_count {name_count}")
    print(
        f"[fake-bnet] CLIENT_STATSREQ name_count={name_count} "
        f"key_count={key_count}",
        flush=True,
    )
    reply = struct.pack("<III", 1, key_count, STATSREQ_REQUEST_ID)
    for i in range(key_count):
        val = STATS_VALUES[i % len(STATS_VALUES)]
        reply += val.encode("ascii") + b"\x00"
    send_bnet(conn, SERVER_STATSREPLY, reply)
    print("[fake-bnet] -> SERVER_STATSREPLY", flush=True)


def serve_one(port: int, mode: str) -> int:
    import socket as _socket
    with listen_one(port, label=f"fake-bnet:{mode}") as conn:
        if mode == "chat":
            run_chat(conn)
        elif mode == "stats":
            run_stats(conn)
        else:
            raise RuntimeError(f"unknown mode {mode!r}")

        try:
            conn.shutdown(_socket.SHUT_WR)
        except OSError:
            pass
        # Drain whatever the client may still send so the close is
        # graceful.  Ignore timeouts/errors.
        try:
            conn.settimeout(2.0)
            while True:
                data = conn.recv(4096)
                if not data:
                    break
        except OSError:
            pass
    return 0


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=6901)
    p.add_argument("--mode", choices=["chat", "stats"], default="chat")
    args = p.parse_args(argv)
    try:
        return serve_one(args.port, args.mode)
    except Exception as exc:  # noqa: BLE001 -- diagnostic
        print(f"[fake-bnet] error: {exc}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
