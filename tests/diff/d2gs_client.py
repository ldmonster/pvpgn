# SPDX-License-Identifier: GPL-2.0-or-later
"""Minimal mock D2GS (Diablo II Game Server) client of the D2CS realm server.

A real D2GS opens a server-to-server link to d2cs: it connects, sends the
one-byte init class (CLIENT_INITCONN_CLASS_D2GS = 0x64), and d2cs immediately
replies with an AUTHREQ. The D2GS answers with an AUTHREPLY (version + checksum
+ a 128-byte signature); with d2gs_version=0 / d2gs_checksum=0 (the defaults)
d2cs accepts any values and replies AUTHREPLY(reply=SUCCEED).

Framing (src/common/d2cs_d2gs_protocol.h t_d2cs_d2gs_header, all little-endian):
    [u16 size][u16 type][u32 seqno]   — size INCLUDES the 8-byte header.

Only the connect+auth handshake is modelled here (enough to exercise the
init-class-0x64 D2GS-link path); game create/join routing is not.
"""
import socket
import struct

CLIENT_INITCONN_CLASS_D2GS = 0x64

# d2cs <-> d2gs opcodes (shared type byte per direction).
D2CS_D2GS_AUTHREQ   = 0x10
D2GS_D2CS_AUTHREPLY = 0x11   # d2gs -> d2cs (version/checksum/sign)
D2CS_D2GS_AUTHREPLY = 0x11   # d2cs -> d2gs (reply code)
D2CS_D2GS_SETGSINFO = 0x12
D2CS_D2GS_ECHOREQ   = 0x13

AUTHREPLY_SUCCEED      = 0x00
AUTHREPLY_BAD_VERSION  = 0x01
AUTHREPLY_BAD_CHECKSUM = 0x02

HEADER = struct.Struct("<HHI")  # size, type, seqno


def _cstr(s: str) -> bytes:
    return s.encode("latin-1", "replace") + b"\x00"


class D2gsClient:
    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b""
        self._init_sent = False

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    # -- framing ----------------------------------------------------------
    def send_init(self):
        if not self._init_sent:
            self.sock.sendall(bytes([CLIENT_INITCONN_CLASS_D2GS]))
            self._init_sent = True

    def send(self, ptype: int, body: bytes = b"", seqno: int = 0):
        if not self._init_sent:
            self.send_init()
        size = HEADER.size + len(body)
        self.sock.sendall(HEADER.pack(size, ptype, seqno) + body)

    def _fill(self, n: int) -> bool:
        while len(self.buf) < n:
            try:
                chunk = self.sock.recv(4096)
            except OSError:
                return False
            if not chunk:
                return False
            self.buf += chunk
        return True

    def recv(self):
        """Return (type, seqno, body) of the next framed packet, or None."""
        if not self._fill(HEADER.size):
            return None
        size, ptype, seqno = HEADER.unpack_from(self.buf, 0)
        if size < HEADER.size:
            return None
        if not self._fill(size):
            return None
        body = self.buf[HEADER.size:size]
        self.buf = self.buf[size:]
        return ptype, seqno, body

    def recv_type(self, want: int, max_packets: int = 10):
        for _ in range(max_packets):
            r = self.recv()
            if r is None:
                return None
            ptype, seqno, body = r
            if ptype == want:
                return (seqno, body)
        return None

    # -- handshake --------------------------------------------------------
    def recv_authreq(self):
        """Receive D2CS_D2GS_AUTHREQ (0x10). Returns dict(sessionnum, signlen,
        realmname) or None."""
        r = self.recv_type(D2CS_D2GS_AUTHREQ)
        if r is None:
            return None
        _seqno, body = r
        if len(body) < 8:
            return None
        sessionnum, signlen = struct.unpack_from("<II", body, 0)
        rest = body[8:]
        nul = rest.find(b"\x00")
        realmname = (rest[:nul] if nul >= 0 else rest).decode("latin-1", "replace")
        return {"sessionnum": sessionnum, "signlen": signlen,
                "realmname": realmname}

    def send_authreply(self, version: int = 0, checksum: int = 0,
                       randnum: int = 0):
        """Send D2GS_D2CS_AUTHREPLY (0x11): version, checksum, randnum, signlen,
        sign[128]. The oracle expects exactly sizeof(t_d2gs_d2cs_authreply)."""
        body = struct.pack("<IIII", version, checksum, randnum, 0) + bytes(128)
        self.send(D2GS_D2CS_AUTHREPLY, body)

    def recv_auth_result(self):
        """Receive the d2cs AUTHREPLY (0x11) result code, or None."""
        r = self.recv_type(D2CS_D2GS_AUTHREPLY)
        if r is None:
            return None
        _seqno, body = r
        if len(body) < 4:
            return None
        return struct.unpack_from("<I", body, 0)[0]

    def handshake(self):
        """Run the full connect+auth handshake. Returns dict with the AUTHREQ
        fields and the resulting reply code, or None on failure."""
        self.send_init()
        req = self.recv_authreq()
        if req is None:
            return None
        self.send_authreply()
        reply = self.recv_auth_result()
        return {"authreq": req, "reply": reply}
