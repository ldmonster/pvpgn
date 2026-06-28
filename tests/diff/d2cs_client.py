# SPDX-License-Identifier: GPL-2.0-or-later
"""Protocol-faithful mock D2CS (Diablo II Character Server) client.

Drives the realm/character flow a real D2 client speaks AFTER its BNCS login:
the client opens a TCP connection to d2cs (default port 6113), sends a single
init class byte (CLIENT_INITCONN_CLASS_D2CS = 0x01), then framed packets.

Framing (src/common/d2cs_protocol.h t_d2cs_client_header):
    [u16 size LE][u8 type]   — size INCLUDES the 3-byte header; little-endian.

Opcodes implemented here (client->server / server->client share the type byte):
    0x01 LOGINREQ / LOGINREPLY
    0x02 CREATECHARREQ / CREATECHARREPLY
    0x17 CHARLISTREQ / CHARLISTREPLY

This mock matches the ORIGINAL server's wire layout (the oracle); it is used to
both differentially test against the original d2cs and conformance-test v3.
"""
import socket
import struct

import bncs_client as _bc  # for blizzard_hash (shared with the v3 token derivation)

# Shared realm secret — must match kRealmKey in d2cs_tcp_session.cpp. The v3 d2cs
# validates a LOGINREQ by recomputing blizzard_hash(key ‖ account ‖ sessionnum ‖
# seqno) and comparing it to secret_hash; a mock standing in for the realm-join
# that issued the token computes the same value here.
D2CS_REALM_KEY = b"pvpgn-v3-d2cs-realm-secret-v1"


def d2cs_token(account: str, sessionnum: int, seqno: int) -> bytes:
    """The 20-byte keyed LOGINREQ token the v3 d2cs expects (5 LE words)."""
    buf = (D2CS_REALM_KEY + account.encode("latin-1") +
           struct.pack("<II", sessionnum, seqno))
    return struct.pack("<5I", *_bc.blizzard_hash(buf))

# Connection init class byte (init_protocol.h CLIENT_INITCONN_CLASS_D2CS).
INIT_D2CS = 0x01

# Packet type bytes (d2cs_protocol.h).
D2CS_LOGINREQ      = 0x01
D2CS_LOGINREPLY    = 0x01
D2CS_CREATECHARREQ = 0x02
D2CS_CREATECHARREPLY = 0x02
D2CS_CHARLISTREQ   = 0x17
D2CS_CHARLISTREPLY = 0x17

# LOGINREPLY result codes.
LOGINREPLY_SUCCEED = 0x00
LOGINREPLY_BADPASS = 0x0c

# CREATECHARREPLY result codes.
CREATECHAR_SUCCEED      = 0x00
CREATECHAR_FAILED       = 0x01
CREATECHAR_ALREADY_EXIST = 0x14
CREATECHAR_NAME_REJECT  = 0x15


def _cstr(s: str) -> bytes:
    return s.encode("latin-1", "replace") + b"\x00"


class D2csClient:
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
    def _send_init(self):
        if not self._init_sent:
            self.sock.sendall(bytes([INIT_D2CS]))
            self._init_sent = True

    def send(self, ptype: int, body: bytes = b""):
        """Send a framed d2cs packet (size includes the 3-byte header)."""
        self._send_init()
        size = len(body) + 3
        self.sock.sendall(struct.pack("<HB", size, ptype) + body)

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
        """Return (type, body) of the next packet, or None on close/timeout."""
        if not self._fill(3):
            return None
        size, ptype = struct.unpack_from("<HB", self.buf, 0)
        if size < 3:
            return None
        if not self._fill(size):
            return None
        body = self.buf[3:size]
        self.buf = self.buf[size:]
        return ptype, body

    def recv_type(self, want: int, max_packets: int = 10):
        for _ in range(max_packets):
            r = self.recv()
            if r is None:
                return None
            ptype, body = r
            if ptype == want:
                return body
        return None

    # -- requests ---------------------------------------------------------
    def login(self, account: str, sessionnum: int = 1, sessionkey: int = 0,
              clienttag: str = "D2DV", secret_hash=(0, 0, 0, 0, 0),
              secret_hash_raw=None, seqno: int = 0):
        """CLIENT_D2CS_LOGINREQ (0x01). Full fixed block (11 u32 + 5 u32 hash)
        then the account name. `secret_hash_raw` (20 bytes from the BNCS
        realm-join reply) is forwarded verbatim when provided. `seqno` MUST equal
        the realm-join request seqno — bnetd issued the secret_hash using it as
        the salt and re-derives the salt from it on validation. Returns the u32
        reply code or None."""
        ctag = struct.unpack("<I", clienttag.encode("latin-1")[:4].ljust(4, b"\0"))[0]
        body = struct.pack(
            "<11I",
            seqno,        # seqno (salt — must match realm-join seqno)
            0,            # u1
            0,            # bncs_addr1
            sessionnum,   # sessionnum
            sessionkey,   # sessionkey (client-defined, usually 0)
            0,            # cdkey_id
            0,            # u5
            ctag,         # clienttag
            0,            # bnversion
            0,            # bncs_addr2
            0,            # u6
        )
        if secret_hash_raw is not None:
            body += secret_hash_raw[:20].ljust(20, b"\x00")
        else:
            body += struct.pack("<5I", *secret_hash)
        body += _cstr(account)
        self.send(D2CS_LOGINREQ, body)
        rep = self.recv_type(D2CS_LOGINREPLY)
        if rep is None or len(rep) < 4:
            return None
        return struct.unpack_from("<I", rep, 0)[0]

    def create_char(self, name: str, char_class: int = 1, status: int = 0):
        """CLIENT_D2CS_CREATECHARREQ (0x02): chclass(u16) u1(u16) status(u16)
        + char name. Returns the u32 reply code or None."""
        body = struct.pack("<HHH", char_class, 0, status) + _cstr(name)
        self.send(D2CS_CREATECHARREQ, body)
        rep = self.recv_type(D2CS_CREATECHARREPLY)
        if rep is None or len(rep) < 4:
            return None
        return struct.unpack_from("<I", rep, 0)[0]

    def char_list(self, maxchar: int = 8):
        """CLIENT_D2CS_CHARLISTREQ (0x17). Returns a dict with the reply
        header counts and the list of character names, or None."""
        body = struct.pack("<HH", maxchar, 0)
        self.send(D2CS_CHARLISTREQ, body)
        rep = self.recv_type(D2CS_CHARLISTREPLY)
        if rep is None or len(rep) < 8:
            return None
        maxc, currchar, u1, currchar2 = struct.unpack_from("<HHHH", rep, 0)
        names = []
        pos = 8
        for _ in range(currchar):
            nul = rep.find(b"\x00", pos)
            if nul < 0:
                break
            names.append(rep[pos:nul].decode("latin-1", "replace"))
            pos = nul + 1
            pos += 33  # portrait block follows each name
        return {
            "maxchar": maxc,
            "currchar": currchar,
            "u1": u1,
            "currchar2": currchar2,
            "names": names,
        }
