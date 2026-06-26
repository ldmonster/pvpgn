#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""A protocol-faithful BNFTP (Battle.net File Transfer) mock client.

Drives BOTH the original pvpgn-server (oracle) and the v3 rewrite over the
shared BNet/BNFTP port. BNFTP is selected by the init byte 0x02
(CLIENT_INITCONN_CLASS_FILE) instead of 0x01 (BNCS).

Wire format reverse-engineered from:
  oracle: src/common/file_protocol.h, src/bnetd/{handle_file,file}.cpp
  v3:     src/protocol/file/{wire_types.hpp,codec.cpp,src/bnftp_fsm.cpp}

CLIENT_FILE_REQ (type 0x0100), all little-endian:
    u16 size          total packet length (incl. these 4 header bytes)
    u16 type          0x0100
    u32 archtag       e.g. "68XI" (IX86 reversed)
    u32 clienttag     e.g. "PXES" (SEXP reversed)
    u32 adid
    u32 extensiontag
    u32 startoffset
    u64 timestamp
    cstring filename  NUL-terminated
  -> fixed prefix = 32 bytes, then filename + NUL.

SERVER_FILE_REPLY (type 0x0000), all little-endian:
    u16 size          total header-packet length (incl. these 4 header bytes)
    u16 type          0x0000
    u32 filelen       number of raw data bytes that follow this header packet
    u32 adid          echoed
    u32 extensiontag  echoed
    u64 timestamp     file mtime (Win FILETIME on v3 / bnettime on oracle)
    cstring filename  echoed (the requested rawname), NUL-terminated
  -> fixed prefix = 24 bytes, then filename + NUL, then `filelen` raw data bytes.
"""
import socket
import struct

CLIENT_FILE_REQ = 0x0100
SERVER_FILE_REPLY = 0x0000

ARCHTAG = b"IX86"[::-1]   # "68XI"
CLIENTTAG = b"SEXP"[::-1]  # "PXES"


def build_file_req(filename: str, adid=0, extensiontag=0, startoffset=0,
                   timestamp=0, archtag=ARCHTAG, clienttag=CLIENTTAG,
                   override_size=None):
    """Build a CLIENT_FILE_REQ packet body (without the leading 0x02 init byte)."""
    fname = filename.encode("latin-1") + b"\x00"
    body = archtag + clienttag
    body += struct.pack("<III", adid, extensiontag, startoffset)
    body += struct.pack("<Q", timestamp)
    body += fname
    size = 4 + len(body) if override_size is None else override_size
    return struct.pack("<HH", size, CLIENT_FILE_REQ) + body


class BnftpClient:
    def __init__(self, host: str, port: int, timeout: float = 4.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        # BNFTP protocol-select byte (CLIENT_INITCONN_CLASS_FILE).
        self.sock.sendall(b"\x02")

    def send_raw(self, data: bytes):
        self.sock.sendall(data)

    def request_file(self, filename: str, **kw):
        self.send_raw(build_file_req(filename, **kw))

    def send_packet(self, pkt: bytes):
        self.send_raw(pkt)

    def recv_all(self, max_bytes=1 << 20):
        """Drain bytes until close or timeout. Returns (data, closed_bool)."""
        data = b""
        closed = False
        try:
            while len(data) < max_bytes:
                chunk = self.sock.recv(4096)
                if not chunk:
                    closed = True
                    break
                data += chunk
        except socket.timeout:
            closed = False
        except (ConnectionError, OSError):
            closed = True
        return data, closed

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def parse_reply(data: bytes):
    """Parse a SERVER_FILE_REPLY header + trailing raw data from `data`.

    Returns a dict or None if there is no parseable header.
    """
    if len(data) < 4:
        return None
    size, typ = struct.unpack_from("<HH", data, 0)
    if size < 4 or len(data) < size:
        return {"raw_len": len(data), "size": size, "type": typ,
                "truncated": True}
    if typ != SERVER_FILE_REPLY:
        return {"raw_len": len(data), "size": size, "type": typ,
                "unknown_type": True}
    filelen, adid, extensiontag = struct.unpack_from("<III", data, 4)
    timestamp = struct.unpack_from("<Q", data, 16)[0]
    nul = data.find(b"\x00", 24)
    filename = data[24:nul].decode("latin-1", "replace") if nul >= 0 else ""
    header_end = (nul + 1) if nul >= 0 else size
    file_data = data[header_end:]
    return {
        "size": size, "type": typ, "filelen": filelen, "adid": adid,
        "extensiontag": extensiontag, "timestamp": timestamp,
        "filename": filename, "header_end": header_end,
        "file_data": file_data, "data_bytes": len(file_data),
        "raw_len": len(data),
    }
