"""Shared wire helpers for the v3 client e2e smoke mocks.

Both ``fake_bnftp_server.py`` and ``fake_bnet_server.py`` use this
module to:

* bind a single-shot TCP listener on ``127.0.0.1``,
* exchange BNet-class packets ({type LE u16, size LE u16}, body),
* drive the standard pre-login handshake
  (AUTH_INFO / AUTHREQ_109 / ICONREQ / ICONREPLY).

Keep this module dependency-free (stdlib only) and small -- it is
deliberately not a general-purpose framework.
"""

from __future__ import annotations

import socket
import struct
from typing import Tuple


# ---- BNet packet types referenced by every mock --------------------------

CLIENT_AUTH_INFO     = 0x50ff
SERVER_AUTHREQ_109   = 0x50ff
CLIENT_ICONREQ       = 0x2dff
SERVER_ICONREPLY     = 0x2dff


# ---- raw I/O -------------------------------------------------------------


def recv_exact(sock: socket.socket, n: int) -> bytes:
    """Block until *exactly* ``n`` bytes have been read.  Raises on EOF."""
    out = bytearray()
    while len(out) < n:
        chunk = sock.recv(n - len(out))
        if not chunk:
            raise RuntimeError(
                f"short read: wanted {n}, got {len(out)} (eof)"
            )
        out.extend(chunk)
    return bytes(out)


# ---- BNet framing --------------------------------------------------------


def recv_bnet(sock: socket.socket) -> Tuple[int, bytes]:
    """Read one BNet packet.  Returns (ptype, body_bytes)."""
    hdr = recv_exact(sock, 4)
    ptype, psize = struct.unpack("<HH", hdr)
    if psize < 4:
        raise RuntimeError(f"bad bnet size {psize}")
    body = recv_exact(sock, psize - 4) if psize > 4 else b""
    return ptype, body


def send_bnet(sock: socket.socket, ptype: int, body: bytes = b"") -> None:
    """Send one BNet packet.  Computes the size field for you."""
    size = 4 + len(body)
    sock.sendall(struct.pack("<HH", ptype, size) + body)


def expect(sock: socket.socket, want_type: int, label: str) -> bytes:
    """Read one BNet packet and assert its type matches ``want_type``.

    Logs a one-line summary on success and raises on mismatch.
    """
    ptype, body = recv_bnet(sock)
    if ptype != want_type:
        raise RuntimeError(
            f"expected {label} (0x{want_type:04x}), got 0x{ptype:04x}"
        )
    print(
        f"[mock] <- {label} (0x{ptype:04x}, {len(body) + 4} B)",
        flush=True,
    )
    return body


# ---- listener / handshake ------------------------------------------------


def listen_one(port: int, label: str = "mock") -> socket.socket:
    """Bind on 127.0.0.1:``port``, listen(1), accept exactly one
    connection and return the accepted socket.

    Caller is responsible for closing it.  The listening socket is
    closed before this function returns -- a second client would be
    refused.
    """
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind(("127.0.0.1", port))
        srv.listen(1)
        print(
            f"[{label}] listening on 127.0.0.1:{port}",
            flush=True,
        )
        conn, addr = srv.accept()
        print(f"[{label}] connection from {addr}", flush=True)
        return conn
    finally:
        srv.close()


def do_handshake(conn: socket.socket, *, init_class: int = 0x01) -> None:
    """Drive the BNet pre-login handshake against ``conn``:

      C -> S : <init_class>                          1 B
      C -> S : CLIENT_AUTH_INFO    (0x50ff)
      S -> C : SERVER_AUTHREQ_109  (0x50ff)
      C -> S : CLIENT_ICONREQ      (0x2dff)
      S -> C : SERVER_ICONREPLY    (0x2dff)

    The fake server values are throwaway (sessionkey=0xdeadbeef,
    sessionnum=1, empty cstrs) -- we only validate packet types,
    not body contents.
    """
    cls = recv_exact(conn, 1)
    if cls != bytes([init_class]):
        raise RuntimeError(
            f"bad init class 0x{cls.hex()} (want 0x{init_class:02x})"
        )

    expect(conn, CLIENT_AUTH_INFO, "CLIENT_AUTH_INFO")
    body = (
        struct.pack("<III", 0, 0xDEADBEEF, 1)
        + struct.pack("<Q", 0)
        + b"\x00\x00"
    )
    send_bnet(conn, SERVER_AUTHREQ_109, body)
    print("[mock] -> SERVER_AUTHREQ_109", flush=True)

    expect(conn, CLIENT_ICONREQ, "CLIENT_ICONREQ")
    send_bnet(conn, SERVER_ICONREPLY, struct.pack("<Q", 0) + b"\x00")
    print("[mock] -> SERVER_ICONREPLY", flush=True)
