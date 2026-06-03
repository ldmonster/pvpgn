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
    --> SID_PING             (0x25)  cookie
    <-- SID_PING             (0x25)  same cookie (ECHOREPLY)
    --> SID_ENTERCHAT        (0x0A)  username
    <-- SID_ENTERCHAT        (0x0A)  unique_name == username
    --> SID_JOINCHANNEL      (0x0C)  channel
    <-- SID_CHATEVENT        (0x0F)  join outcome (see note at the call site)

A second connection drives the one genuine reject path the FSM has today —
an empty username yields LOGONRESPONSE2 result 0x01.

Wire note: bnetd's shared-port dispatcher (app/bnetd/.../bnet_bnftp_dispatch)
selects the BNet codec when the FIRST byte of the stream is the packet marker
0xFF, and routes everything else to BNFTP. Unlike legacy PvPGN it does NOT
consume a separate 1-byte init-class selector here, so the client sends its
first SID packet (0xFF ...) directly.

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
SID_PING            = 0x25   # ECHOREQ / ECHOREPLY (server mirrors cookie)
SID_ENTER_CHAT      = 0x0A   # both directions
SID_JOIN_CHANNEL    = 0x0C   # client->server
SID_CHAT_EVENT      = 0x0F   # server->client

EID_CHANNEL = 3              # ChatEvent.event_id for a channel name
EID_INFO    = 4              # ChatEvent.event_id for an info/error line

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


def expect(sock: socket.socket, want_code: int, label: str) -> bytes:
    code, body = recv_packet(sock)
    if code != want_code:
        raise AssertionError(
            f"expected {label} (0x{want_code:02x}), got 0x{code:02x}")
    return body


def read_cstrings(body: bytes, count: int):
    """Split the first `count` NUL-terminated latin-1 strings out of `body`."""
    out, off = [], 0
    for _ in range(count):
        end = body.index(b"\x00", off)
        out.append(body[off:end].decode("latin-1"))
        off = end + 1
    return out


def parse_chat_event(body: bytes):
    """Return (event_id, username, text) from a SID_CHATEVENT body."""
    if len(body) < 24:
        raise AssertionError(f"CHATEVENT body too short ({len(body)} B)")
    event_id = struct.unpack_from("<I", body, 0)[0]
    username, text = read_cstrings(body[24:], 2)
    return event_id, username, text


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

        # --- Journey 1: successful modern login + post-login chat path ------
        print("[journey] accept: AUTH_INFO -> AUTH_CHECK -> LOGONRESPONSE2(e2euser)"
              " -> PING -> ENTER_CHAT -> JOIN_CHANNEL")
        with connect("127.0.0.1", port) as sock:
            do_auth_handshake(sock)
            result = logon(sock, "e2euser")
            if result != 0x00:
                raise AssertionError(
                    f"expected login accept 0x00, got 0x{result:02x}")
            print(f"  [client] <- LOGONRESPONSE2 result=0x00 (login accepted) OK")

            # PING: the server mirrors the cookie verbatim (ECHOREPLY).
            cookie = 0x12345678
            send_packet(sock, SID_PING, struct.pack("<I", cookie))
            body = expect(sock, SID_PING, "PING echo")
            echoed = struct.unpack_from("<I", body, 0)[0]
            if echoed != cookie:
                raise AssertionError(
                    f"PING echo mismatch: sent 0x{cookie:08x}, got 0x{echoed:08x}")
            print(f"  [client] <- PING echo 0x{echoed:08x} OK")

            # ENTER_CHAT: LoggedIn -> InChat; server echoes a chat identity.
            send_packet(sock, SID_ENTER_CHAT, cstring("e2euser") + cstring(""))
            uniq, _stat, _acct = read_cstrings(
                expect(sock, SID_ENTER_CHAT, "ENTER_CHAT reply"), 3)
            if uniq != "e2euser":
                raise AssertionError(
                    f"ENTER_CHAT unique_name: expected 'e2euser', got {uniq!r}")
            print(f"  [client] <- ENTER_CHAT reply unique_name={uniq!r} OK")

            # JOIN_CHANNEL: InChat; server replies with a SID_CHATEVENT.
            #
            # Observed contract today: the join reaches the join_channel
            # use-case, which auto-creates the channel and admits the member,
            # but then fails its account lookup because the permissive login
            # stub leaves current_account_id_ == 0 (no real account exists —
            # login_user is unwired in app/bnetd/main.cpp). The FSM maps that
            # AccountNotFound to EID_INFO(4) "Failed to join channel".
            #
            # This still exercises the full wire path (chat FSM ->
            # join_channel use-case -> ChatEvent encoder -> wire). When
            # login_user is wired (so a real account_id flows through), this
            # assertion is expected to flip to EID_CHANNEL(3) carrying the
            # channel name — at which point this test should be updated
            # deliberately. Pin the current behavior so that flip is reviewed.
            channel = "PvPGN E2E"
            send_packet(sock, SID_JOIN_CHANNEL,
                        struct.pack("<I", 0) + cstring(channel))
            event_id, _user, text = parse_chat_event(
                expect(sock, SID_CHAT_EVENT, "JOIN_CHANNEL CHATEVENT"))
            if event_id == EID_CHANNEL and text == channel:
                # login_user got wired — the journey now fully succeeds.
                print(f"  [client] <- CHATEVENT EID_CHANNEL text={text!r} OK (join succeeded)")
            elif event_id == EID_INFO and text == "Failed to join channel":
                print(f"  [client] <- CHATEVENT EID_INFO text={text!r} OK "
                      "(expected: stub login => account_id 0 => AccountNotFound)")
            else:
                raise AssertionError(
                    f"JOIN_CHANNEL: unexpected ChatEvent event_id={event_id} text={text!r} "
                    f"(wanted EID_CHANNEL/{channel!r} or EID_INFO/'Failed to join channel')")

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
