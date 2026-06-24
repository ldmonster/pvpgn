#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""E2E: accounts created over the wire survive a bnetd restart (file backend).

Complements modern_login_journey_test.py (which runs the inmemory backend, so
state is lost on exit). Here bnetd runs with `[persistence] backend="file"`,
which stores each account as `<data-dir>/<name>.plain`. The test:

    1. spawn bnetd #1 (file backend, workdir W) -> CREATEACCTREQ1(durableuser)
    2. assert <W>/durableuser.plain now exists, then stop bnetd #1
    3. spawn bnetd #2 in the SAME workdir W -> LOGONRESPONSE2(durableuser)
       authenticates against the reloaded account -> 0x00

A negative control on bnetd #2 (unknown user -> 0x01) guards against the file
repo silently "accepting everyone".

Reuses the wire/harness helpers from modern_login_journey_test so the framing
lives in one place. Self-contained: stdlib only, no docker.

Usage: account_persistence_test.py --bnetd <path> [--port N]
"""

from __future__ import annotations

import argparse
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import modern_login_journey_test as mlj  # noqa: E402

USER = "durableuser"


def run_phase(bnetd, port, workdir, fn):
    """Spawn a file-backed bnetd in `workdir`, run fn(sock), tear it down."""
    proc, logf = mlj.spawn_bnetd(bnetd, port, workdir, backend="file")
    try:
        mlj.wait_ready(proc, logf, port)
        with mlj.connect("127.0.0.1", port) as sock:
            mlj.do_auth_handshake(sock)
            return fn(sock)
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except Exception:
                proc.kill()
        logf.close()


def free_port() -> int:
    import socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bnetd", default=None)
    args = ap.parse_args()
    bnetd = mlj.find_bnetd(args.bnetd)
    workdir = tempfile.mkdtemp(prefix="pvpgn-e2e-persist-")

    try:
        # Create the account, then stop bnetd.
        rc = run_phase(bnetd, free_port(), workdir,
                       lambda s: mlj.create_account(s, USER))
        if rc != mlj.CREATE_ACCT1_OK:
            raise AssertionError(f"CREATEACCT1 expected OK, got {rc}")
        print(f"[persist] phase 1: created {USER!r} (bnetd #1 stopped)")

        # The account must be on disk independent of any running server.
        plain = os.path.join(workdir, f"{USER}.plain")
        if not os.path.isfile(plain):
            raise AssertionError(f"expected persisted account file {plain}")
        print(f"[persist] on disk: {os.path.basename(plain)} OK")

        # A fresh bnetd in the same workdir must authenticate it.
        result = run_phase(bnetd, free_port(), workdir,
                           lambda s: mlj.logon(s, USER, mlj.PASSWORD_WORDS))
        if result != 0x00:
            raise AssertionError(
                f"post-restart login expected 0x00, got 0x{result:02x}")
        print(f"[persist] phase 2: login after restart -> 0x00 OK")

        # Negative control: an unknown user is still rejected by the reloaded
        # file repo (it didn't just accept everyone).
        result = run_phase(bnetd, free_port(), workdir,
                           lambda s: mlj.logon(s, "neverexisted", mlj.PASSWORD_WORDS))
        if result != 0x01:
            raise AssertionError(
                f"unknown user after restart expected 0x01, got 0x{result:02x}")
        print(f"[persist] phase 2: unknown user -> 0x01 OK")

        print("[harness] account persistence PASSED")
        return 0
    except Exception as exc:
        print(f"[harness] FAILED: {exc}", file=sys.stderr)
        log = os.path.join(workdir, "bnetd.log")
        if os.path.isfile(log):
            with open(log) as f:
                sys.stderr.write("---- bnetd.log ----\n" + f.read())
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
