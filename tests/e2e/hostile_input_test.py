#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""E2E robustness: real bnetd must survive hostile / malformed wire input.

The three SIGSEGVs found earlier all came from *well-formed* traffic, so
malformed input is at least as likely to expose crashes, hangs, or UB. This
test fires a battery of hostile byte sequences at a real bnetd — each on its
own fresh connection — and asserts:

  * bnetd does not crash (process stays alive after every case), and
  * bnetd keeps serving: a clean AUTH_INFO handshake still succeeds afterward.

It deliberately does NOT assert specific replies to the malformed input — a
graceful reply, a clean close, or a silent drop are all acceptable. The only
contracts are "stays up" and "still serves". Run under the asan/ubsan presets
this doubles as a sanitizer target for the framing/dispatch/decode pipeline.

Self-contained: stdlib only, no docker. Usage:
    hostile_input_test.py --bnetd <path> [--port N]
"""

from __future__ import annotations

import argparse
import os
import socket
import struct
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modern_login_journey_test as mlj  # noqa: E402


def frame(code: int, body: bytes = b"", size: int | None = None) -> bytes:
    """Build a BNet frame; `size` overrides the real length for bad-size cases."""
    return struct.pack("<BBH", 0xFF, code, 4 + len(body) if size is None else size) + body


def hostile_cases() -> list[tuple[str, bytes]]:
    # A well-formed LOGONRESPONSE2 frame (used out-of-order, before AUTH_INFO).
    logon_body = struct.pack("<2I5I", 0, 0, 0, 0, 0, 0, 0) + b"x\x00"
    auth_body = struct.pack("<9I", 0, 0, 0, 0, 0, 0, 0, 0, 0) + b"US\x00" + b"US\x00"
    return [
        ("truncated header (2 bytes)",          b"\xff\x50"),
        ("header size < 4",                      frame(0x50, b"", size=2)),
        ("header size == 0",                     struct.pack("<BBH", 0xFF, 0x50, 0)),
        ("huge size field, empty body",          frame(0x50, b"", size=0xFFFF)),
        ("huge size field, partial body",        frame(0x50, b"\x41" * 200, size=0xFFFF)),
        ("unknown SID, valid frame",             frame(0xEE)),
        ("AUTH_INFO body truncated",             frame(0x50, b"\x00" * 10, size=58)),
        ("AUTH_INFO body too short for fields",  frame(0x50, b"\x00" * 4)),
        ("PING with no ticks",                   frame(0x25)),
        ("out-of-order LOGONRESPONSE2 first",    frame(0x3A, logon_body)),
        ("non-0xFF first byte (BNFTP/garbage)",  b"\x99\x01\x02\x03\x04\x05"),
        ("flood of unknown SIDs",                frame(0xEE) * 64),
        ("valid AUTH_INFO + garbage trailer",    frame(0x50, auth_body) + b"\xde\xad\xbe\xef"),
        ("single zero byte",                     b"\x00"),
        ("0xFF then immediate close",            b"\xff"),
    ]


def fire(host: str, port: int, payload: bytes) -> None:
    """Send one hostile payload on a fresh connection; tolerate any outcome."""
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            s.sendall(payload)
            # Give the server a moment; drain whatever it sends (or EOF/timeout).
            try:
                s.recv(256)
            except (socket.timeout, OSError):
                pass
        finally:
            s.close()
    except OSError:
        # Connection refused mid-run would mean a crash; the caller's liveness
        # check catches that. A reset/broken-pipe here is an acceptable outcome.
        pass


def alive(proc) -> bool:
    return proc.poll() is None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bnetd", default=None)
    ap.add_argument("--port", type=int, default=0)
    args = ap.parse_args()
    bnetd = mlj.find_bnetd(args.bnetd)

    port = args.port or 0
    if port == 0:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind(("127.0.0.1", 0))
            port = s.getsockname()[1]

    workdir = tempfile.mkdtemp(prefix="pvpgn-e2e-hostile-")
    proc, logf = mlj.spawn_bnetd(bnetd, port, workdir)
    try:
        mlj.wait_ready(proc, logf, port)
        print(f"[harness] bnetd ready on 127.0.0.1:{port} (pid={proc.pid})")

        for label, payload in hostile_cases():
            fire("127.0.0.1", port, payload)
            time.sleep(0.02)  # let any crash surface on the worker thread
            if not alive(proc):
                raise AssertionError(
                    f"bnetd died after hostile case: {label!r} "
                    f"(rc={proc.returncode})")
            print(f"  [hostile] survived: {label}")

        # Liveness proof: a clean modern handshake must still work.
        with mlj.connect("127.0.0.1", port) as sock:
            mlj.do_auth_handshake(sock)
        if not alive(proc):
            raise AssertionError("bnetd died during the post-hostile clean handshake")
        print("[harness] clean handshake after hostile barrage OK")
        print("[harness] hostile input robustness PASSED")
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
            except Exception:
                proc.kill()
        logf.close()


if __name__ == "__main__":
    raise SystemExit(main())
