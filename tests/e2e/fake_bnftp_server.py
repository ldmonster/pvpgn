#!/usr/bin/env python3
"""Minimal mock for the BNFTP file-class server used by
``scripts/v3-e2e-bnftp-smoke.sh``.

Listens on a TCP port, accepts one connection from ``bnftp``,
performs the legacy single-step file request handshake, and serves
a fixed in-memory payload.  Raw I/O / single-shot listen helpers
live in :mod:`fake_bnet_common`.

Protocol summary (see ``src/common/file_protocol.h`` for the legacy
header; the FILE class uses ``{size, type}`` -- opposite of BNet):

    client -> server : 0x02                       init class byte
    client -> server : { size LE u16, type=0x0100 LE u16 }
                       arch[4], clienttag[4], adid[4], extension[4],
                       startoffset[4], timestamp[8], filename "...\\0"
    server -> client : { size LE u16, type=0x0000 LE u16 }
                       filelen[4], adid[4], extension[4],
                       timestamp[8], filename "...\\0"
    server -> client : raw `filelen` bytes
"""

from __future__ import annotations

import argparse
import struct
import sys

from fake_bnet_common import listen_one, recv_exact


PAYLOAD = b"hello from fake bnftp server\n"


def recv_cstr(sock, limit: int) -> bytes:
    out = bytearray()
    while len(out) < limit:
        b = recv_exact(sock, 1)
        if b == b"\x00":
            return bytes(out)
        out.extend(b)
    raise RuntimeError(f"cstring exceeded limit {limit}")


def serve_one(port: int) -> int:
    with listen_one(port, label="fake-bnftp") as conn:
        # 1) init class byte
        cls = recv_exact(conn, 1)
        if cls != b"\x02":
            raise RuntimeError(
                f"unexpected init class byte 0x{cls.hex()} (want 0x02)"
            )

        # 2) FILE-class header { size, type }
        hdr = recv_exact(conn, 4)
        size, ptype = struct.unpack("<HH", hdr)
        if ptype != 0x0100:
            raise RuntimeError(
                f"unexpected packet type 0x{ptype:04x} (want 0x0100)"
            )
        if size < 4 + 24:
            raise RuntimeError(f"req too small: {size}")

        body = recv_exact(conn, 24)
        archtag, clienttag = body[0:4], body[4:8]
        startoffset = struct.unpack("<I", body[16:20])[0]

        remaining = size - 4 - 24
        if remaining <= 0:
            raise RuntimeError("missing filename")
        raw = recv_exact(conn, remaining)
        filename = raw.split(b"\x00", 1)[0]
        print(
            f"[fake-bnftp] arch={archtag!r} client={clienttag!r} "
            f"startoffset={startoffset} file={filename!r}",
            flush=True,
        )

        # 3) send SERVER_FILE_REPLY
        reply_name = filename + b"\x00"
        reply_body = (
            struct.pack("<III", len(PAYLOAD), 0, 0)
            + struct.pack("<Q", 0)
        )
        reply_size = 4 + len(reply_body) + len(reply_name)
        reply = (
            struct.pack("<HH", reply_size, 0x0000)
            + reply_body
            + reply_name
        )
        conn.sendall(reply)
        conn.sendall(PAYLOAD)
        print("[fake-bnftp] payload sent, closing", flush=True)
    return 0


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=6900)
    args = p.parse_args(argv)
    try:
        return serve_one(args.port)
    except Exception as exc:  # noqa: BLE001 -- diagnostic
        print(f"[fake-bnftp] error: {exc}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
