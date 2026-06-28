# SPDX-License-Identifier: GPL-2.0-or-later
"""Minimal mock D2GS client speaking the D2GS->D2DBS wire protocol.

Framing (all little-endian):
    [size:2][type:2][seqno:4][body...]
where `size` is the total packet length including the 8-byte header.

The connection opens with a single connect-class byte (0x65) before any
framed packet.
"""
import socket
import struct

CONNECT_CLASS_D2GS_TO_D2DBS = 0x65

# message types
SAVE_DATA_REQUEST = 0x30
GET_DATA_REQUEST = 0x31
UPDATE_LADDER = 0x32
CHAR_LOCK = 0x33
ECHO_REPLY = 0x34

SAVE_DATA_REPLY = 0x30
GET_DATA_REPLY = 0x31
ECHO_REQUEST = 0x34

# data-type sub-codes
DATATYPE_CHARSAVE = 0x01
DATATYPE_PORTRAIT = 0x02

# result codes
GET_DATA_SUCCESS = 0
GET_DATA_FAILED = 1
GET_DATA_CHAR_LOCKED = 2
SAVE_DATA_SUCCESS = 0
SAVE_DATA_FAILED = 1


def _cstr(s: str) -> bytes:
    return s.encode("latin-1") + b"\x00"


class D2dbsClient:
    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self._buf = b""
        self._sent_init = False

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    # -- low level ----------------------------------------------------------

    def send_init(self):
        """Send the one-byte connect class that opens the connection."""
        self.sock.sendall(bytes([CONNECT_CLASS_D2GS_TO_D2DBS]))
        self._sent_init = True

    def send_frame(self, ptype: int, seqno: int, body: bytes = b""):
        if not self._sent_init:
            self.send_init()
        size = 8 + len(body)
        pkt = struct.pack("<HHI", size, ptype, seqno) + body
        self.sock.sendall(pkt)

    def recv_frame(self, timeout: float = 3.0):
        """Read one framed packet; return (type, seqno, body) or None on timeout/EOF."""
        self.sock.settimeout(timeout)
        while True:
            if len(self._buf) >= 8:
                size, ptype, seqno = struct.unpack("<HHI", self._buf[:8])
                if size < 8:
                    # malformed length; surface raw for the diff
                    chunk = self._buf
                    self._buf = b""
                    return ("MALFORMED", size, chunk)
                if len(self._buf) >= size:
                    body = self._buf[8:size]
                    self._buf = self._buf[size:]
                    return (ptype, seqno, body)
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                return None
            if not data:
                return None if not self._buf else ("EOF", 0, self._buf)
            self._buf += data

    # -- requests -----------------------------------------------------------

    def get_data(self, seqno: int, account: str, char: str, realm: str,
                 datatype: int = DATATYPE_CHARSAVE):
        body = struct.pack("<H", datatype) + _cstr(account) + _cstr(char) + _cstr(realm)
        self.send_frame(GET_DATA_REQUEST, seqno, body)

    def save_data(self, seqno: int, account: str, char: str, realm: str,
                  blob: bytes, datatype: int = DATATYPE_CHARSAVE):
        body = (struct.pack("<HH", datatype, len(blob))
                + _cstr(account) + _cstr(char) + _cstr(realm) + blob)
        self.send_frame(SAVE_DATA_REQUEST, seqno, body)

    def char_lock(self, seqno: int, account: str, char: str, realm: str,
                  lock: bool = True):
        body = struct.pack("<I", 1 if lock else 0) + _cstr(account) + _cstr(char) + _cstr(realm)
        self.send_frame(CHAR_LOCK, seqno, body)

    def echo_reply(self, seqno: int):
        self.send_frame(ECHO_REPLY, seqno)


def parse_get_data_reply(body: bytes):
    """Parse a GET_DATA_REPLY body (everything after the 8-byte header)."""
    if len(body) < 16:
        return None
    result, creattime, allowladder, datatype, datalen = struct.unpack("<IIIHH", body[:16])
    rest = body[16:]
    cname_end = rest.find(b"\x00")
    if cname_end < 0:
        char_name, blob = rest, b""
    else:
        char_name = rest[:cname_end]
        blob = rest[cname_end + 1:]
    return {
        "result": result,
        "charcreatetime": creattime,
        "allowladder": allowladder,
        "datatype": datatype,
        "datalen": datalen,
        "char_name": char_name.decode("latin-1", "replace"),
        "blob": blob,
    }


def parse_save_data_reply(body: bytes):
    if len(body) < 6:
        return None
    result, datatype = struct.unpack("<IH", body[:6])
    rest = body[6:]
    cend = rest.find(b"\x00")
    char_name = rest[:cend] if cend >= 0 else rest
    return {
        "result": result,
        "datatype": datatype,
        "char_name": char_name.decode("latin-1", "replace"),
    }
