#!/usr/bin/env python3
"""Minimal mock of the BOT-class telnet surface for the v3 ``bnbot``
smoke test.  After the BOT init handshake (``0x03`` class byte + the
``\\x04\\x00`` protocol marker) the mock sends a fixed banner line
and closes the socket.

Wire-level details are documented inline in ``src/v3/tools/client/
bnbot_v3.cpp``.  This mock validates only the init handshake -- the
rest of the bot protocol is plain telnet bytes either way.
"""

from __future__ import annotations

import argparse
import sys

from fake_bnet_common import listen_one, recv_exact


BANNER = "Welcome from fake bnbot\r\n"


def serve_one(port: int) -> int:
    with listen_one(port, label="fake-bnbot") as conn:
        cls = recv_exact(conn, 1)
        if cls != b"\x03":
            raise RuntimeError(
                f"bad init class 0x{cls.hex()} (want 0x03)"
            )
        marker = recv_exact(conn, 2)
        if marker != b"\x04\x00":
            raise RuntimeError(
                f"bad bot marker {marker!r} (want b'\\x04\\x00')"
            )
        print("[fake-bnbot] handshake ok, sending banner", flush=True)
        conn.sendall(BANNER.encode("ascii"))
    return 0


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=6903)
    args = p.parse_args(argv)
    try:
        return serve_one(args.port)
    except Exception as exc:  # noqa: BLE001 -- diagnostic
        print(f"[fake-bnbot] error: {exc}", file=sys.stderr, flush=True)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
