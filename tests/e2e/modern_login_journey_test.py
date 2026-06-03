#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""End-to-end modern-flow login journey against a *real* bnetd.

This is the inverse of the `fake_bnet_server.py` mocks: here Python is the
**client** and we spawn the real `bnetd` binary (inmemory backend, ephemeral
port), drive the modern SID auth handshake over the wire, and assert the
server's replies.

Journey (modern OLS flow, the one bnetd's FSM implements):

    --> SID_AUTH_INFO        (0x50)
    <-- SID_AUTH_CHECK       (0x51)  result == 0  ("version check passed")
    --> SID_LOGONRESPONSE2   (0x3A)  username="e2euser"
    <-- SID_LOGONRESPONSE2   (0x3A)  result == 0x00  (login accepted)

Wire note: bnetd's shared-port dispatcher (app/bnetd/.../bnet_bnftp_dispatch)
selects the BNet codec when the FIRST byte of the stream is the packet marker
0xFF, and routes everything else to BNFTP. Unlike legacy PvPGN it does NOT
consume a separate 1-byte init-class selector here, so the client sends its
first SID packet (0xFF ...) directly.

A second connection drives the one genuine reject path the FSM has today —
an empty username yields result 0x01.

NOTE (behaviour documented by this test, see docs/refactoring/progress.md):
bnetd's composition root (app/bnetd/main.cpp) does NOT wire the `login_user`
use-case, so `BnetFsm::on(LogonResponse2)` takes its null branch and accepts
any non-empty username with 0x00 — authentication is currently a permissive
stub. This test pins that observable contract so the future wiring of
`login_user` (which will turn 0x00 into 0x01/0x02 for bad credentials) shows
up as a deliberate, reviewed change here rather than a silent drift.

Self-contained: stdlib only, no docker, no network beyond loopback.

Usage:
    modern_login_journey_test.py --bnetd <path-to-bnetd> [--port N]
or set the PVPGN_V3_BNETD environment variable.
"""

from __future__ import annotations

import argparse
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time

# ---- wire constants -------------------------------------------------------

SID_AUTH_INFO       = 0x50   # client->server AuthInfo / server->client reply
SID_AUTH_CHECK      = 0x51   # server->client AuthCheckReply
SID_LOGONRESPONSE2  = 0x3A   # both directions

# AuthInfo product/platform tags (arbitrary but well-formed; the FSM only
# stores the product tag, it does not gate on these in the stub).
PLATFORM_IX86 = 0x49583836  # 'IX86' big-endian packed
PRODUCT_D2DV  = 0x44324456  # 'D2DV'


# ---- BNet framing (marker 0xFF, code u8, size u16 LE; size incl 4B header) -

def recv_exact(sock: socket.socket, n: int) -> bytes:
    out = bytearray()
    while len(out) < n:
        chunk = sock.recv(n - len(out))
        if not chunk:
            raise RuntimeError(f"short read: wanted {n}, got {len(out)} (eof)")
        out.extend(chunk)
    return bytes(out)


def send_packet(sock: socket.socket, code: int, body: bytes = b"") -> None:
    size = 4 + len(body)
    if size > 0xFFFF:
        raise ValueError("packet too large")
    sock.sendall(struct.pack("<BBH", 0xFF, code, size) + body)


def recv_packet(sock: socket.socket) -> tuple[int, bytes]:
    hdr = recv_exact(sock, 4)
    marker, code, size = struct.unpack("<BBH", hdr)
    if marker != 0xFF:
        raise RuntimeError(f"bad marker 0x{marker:02x}")
    if size < 4:
        raise RuntimeError(f"bad size {size}")
    body = recv_exact(sock, size - 4) if size > 4 else b""
    return code, body


def cstring(s: str) -> bytes:
    return s.encode("latin-1") + b"\x00"


# ---- message builders / parsers ------------------------------------------

def build_auth_info() -> bytes:
    # 9 u32 fields, then country_abbr + country C-strings.
    fields = struct.pack(
        "<9I",
        0,              # protocol_id
        PLATFORM_IX86,  # platform_id
        PRODUCT_D2DV,   # game_id (product tag)
        0x01000000,     # version_id
        0,              # language_id
        0,              # local_ip
        0,              # tz_bias
        0,              # mpq_locale
        0,              # lang_id
    )
    return fields + cstring("USA") + cstring("United States")


def build_logon_response2(username: str) -> bytes:
    # client_token, server_token, 5x u32 password hash words, username cstr.
    return struct.pack("<2I5I", 0xDEADBEEF, 0, *(0x11111111,) * 5) \
        + cstring(username)


def parse_u32_result(body: bytes) -> int:
    if len(body) < 4:
        raise RuntimeError(f"reply body too short ({len(body)} B)")
    return struct.unpack_from("<I", body, 0)[0]


# ---- one connection's handshake ------------------------------------------

def connect(host: str, port: int, timeout: float = 5.0) -> socket.socket:
    sock = socket.create_connection((host, port), timeout=timeout)
    sock.settimeout(timeout)
    return sock


def do_auth_handshake(sock: socket.socket) -> None:
    """AUTH_INFO -> assert AUTH_CHECK result==0 (first byte 0xFF selects BNet)."""
    send_packet(sock, SID_AUTH_INFO, build_auth_info())
    code, body = recv_packet(sock)
    if code != SID_AUTH_CHECK:
        raise AssertionError(
            f"expected AUTH_CHECK (0x{SID_AUTH_CHECK:02x}), got 0x{code:02x}")
    result = parse_u32_result(body)
    if result != 0:
        raise AssertionError(f"AUTH_CHECK result {result} != 0 (version check failed)")
    print(f"  [client] <- AUTH_CHECK result=0 (version check passed)")


def logon(sock: socket.socket, username: str) -> int:
    send_packet(sock, SID_LOGONRESPONSE2, build_logon_response2(username))
    code, body = recv_packet(sock)
    if code != SID_LOGONRESPONSE2:
        raise AssertionError(
            f"expected LOGONRESPONSE2 (0x{SID_LOGONRESPONSE2:02x}), got 0x{code:02x}")
    return parse_u32_result(body)


# ---- bnetd lifecycle ------------------------------------------------------

def find_bnetd(explicit: str | None) -> str:
    if explicit:
        return explicit
    env = os.environ.get("PVPGN_V3_BNETD")
    if env:
        return env
    # Best-effort fallback: search common build trees.
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(os.path.dirname(here))
    for preset in ("v3-dev", "v3-release", "v3-gcc14", "v3"):
        cand = os.path.join(root, "build", preset, "src", "app", "bnetd", "bnetd")
        if os.path.isfile(cand) and os.access(cand, os.X_OK):
            return cand
    raise SystemExit("bnetd binary not found; pass --bnetd or set PVPGN_V3_BNETD")


def spawn_bnetd(bnetd: str, port: int, workdir: str):
    cfg = os.path.join(workdir, "bnetd.toml")
    with open(cfg, "w") as f:
        f.write('[persistence]\nbackend = "inmemory"\n')
    logf = open(os.path.join(workdir, "bnetd.log"), "w+")
    proc = subprocess.Popen(
        [bnetd, "-c", cfg, "-p", str(port), "-d", workdir, "-l", "info"],
        stdout=logf, stderr=subprocess.STDOUT,
    )
    return proc, logf


def wait_ready(proc, logf, port: int, timeout: float = 15.0) -> None:
    needle = f"listening on 0.0.0.0:{port}"
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            logf.seek(0)
            raise RuntimeError(
                f"bnetd exited early (rc={proc.returncode}):\n{logf.read()}")
        logf.seek(0)
        if needle in logf.read():
            return
        time.sleep(0.1)
    logf.seek(0)
    raise RuntimeError(f"bnetd not ready within {timeout}s:\n{logf.read()}")


# ---- main -----------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bnetd", default=None)
    ap.add_argument("--port", type=int, default=0,
                    help="bnet port; 0 picks a free ephemeral port")
    args = ap.parse_args()

    bnetd = find_bnetd(args.bnetd)

    port = args.port
    if port == 0:
        # Grab a free port, then release it for bnetd to bind.
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]

    workdir = tempfile.mkdtemp(prefix="pvpgn-e2e-")
    proc, logf = spawn_bnetd(bnetd, port, workdir)
    try:
        wait_ready(proc, logf, port)
        print(f"[harness] bnetd ready on 127.0.0.1:{port} (pid={proc.pid})")

        # --- Journey 1: successful modern login -----------------------------
        print("[journey] accept: AUTH_INFO -> AUTH_CHECK -> LOGONRESPONSE2(e2euser)")
        with connect("127.0.0.1", port) as sock:
            do_auth_handshake(sock)
            result = logon(sock, "e2euser")
            if result != 0x00:
                raise AssertionError(
                    f"expected login accept 0x00, got 0x{result:02x}")
            print(f"  [client] <- LOGONRESPONSE2 result=0x00 (login accepted) OK")

        # --- Journey 2: empty-username reject -------------------------------
        print("[journey] reject: empty username -> LOGONRESPONSE2 result=0x01")
        with connect("127.0.0.1", port) as sock:
            do_auth_handshake(sock)
            result = logon(sock, "")
            if result != 0x01:
                raise AssertionError(
                    f"expected reject 0x01 for empty username, got 0x{result:02x}")
            print(f"  [client] <- LOGONRESPONSE2 result=0x01 (rejected) OK")

        print("[harness] modern login journey PASSED")
        return 0
    except Exception as exc:
        print(f"[harness] FAILED: {exc}", file=sys.stderr)
        logf.seek(0)
        sys.stderr.write("---- bnetd.log ----\n" + logf.read())
        return 1
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
        logf.close()


if __name__ == "__main__":
    raise SystemExit(main())
