# SPDX-License-Identifier: GPL-2.0-or-later
"""Launchers for the D2DBS (Diablo II Database Server) under differential test.

`V3D2dbs`       — the v3 rewrite's standalone d2dbs (pvpgn_v3_d2dbs; in-memory
                  character-save + ladder repositories).
`OriginalD2dbs` — the original pvpgn d2dbs binary, in an isolated temp home.
                  d2dbs accepts D2GS connections (gameservlist gates allowed IPs)
                  and needs no bnetd link for the handshake / framing / GET-of-a-
                  missing-character path the diff harness exercises.
"""
import os
import shutil
import socket
import subprocess
import tempfile
import time


class V3D2dbs:
    def __init__(self, d2dbs_bin: str, port: int):
        self.bin = d2dbs_bin
        self.port = port
        self.proc = None
        self.workdir = None
        self.logf = None

    def start(self, timeout: float = 15.0) -> None:
        self.workdir = tempfile.mkdtemp(prefix="pvpgn-v3d2dbs-")
        self.logf = open(os.path.join(self.workdir, "d2dbs.log"), "a+")
        self.proc = subprocess.Popen(
            [self.bin, "--port", str(self.port), "--listen", "127.0.0.1"],
            stdout=self.logf, stderr=subprocess.STDOUT)
        _await_listen(self.proc, self.port, self.logf, "v3 d2dbs", timeout)

    def stop(self) -> None:
        _stop(self.proc)
        if self.logf:
            self.logf.close()
        if self.workdir and os.path.isdir(self.workdir):
            shutil.rmtree(self.workdir, ignore_errors=True)

    def __enter__(self):
        self.start(); return self

    def __exit__(self, *a):
        self.stop()


class OriginalD2dbs:
    """Start the original pvpgn d2dbs in an isolated temp home."""
    def __init__(self, repo: str, port: int):
        self.repo = repo
        self.port = port
        self.proc = None
        self.home = None
        self._logf = None

    def _bin(self) -> str:
        cand = os.path.join(self.repo, "build", "src", "d2dbs", "d2dbs")
        if not os.path.isfile(cand):
            raise FileNotFoundError(f"original d2dbs binary not found: {cand}")
        return cand

    def setup(self) -> None:
        self.home = tempfile.mkdtemp(prefix="pvpgn-origd2dbs-")
        etc = os.path.join(self.home, "etc")
        var = os.path.join(self.home, "var")
        os.makedirs(etc, exist_ok=True)
        for sub in ("charsave", "charinfo", "ladders",
                    "bak/charsave", "bak/charinfo"):
            os.makedirs(os.path.join(var, sub), exist_ok=True)
        src = os.path.join(self.repo, "build", "conf")
        for name in os.listdir(src):
            s = os.path.join(src, name)
            if os.path.isfile(s):
                shutil.copy(s, os.path.join(etc, name))
        self._rewrite(os.path.join(etc, "d2dbs.conf"), var)

    def _rewrite(self, conf_path: str, var: str) -> None:
        with open(conf_path) as f:
            lines = f.readlines()
        out = []
        for line in lines:
            s = line.strip()
            if s.startswith("#") or "=" not in s:
                out.append(line)
                continue
            key = s.split("=", 1)[0].strip()
            if key == "servaddrs":
                out.append(f"servaddrs = 127.0.0.1:{self.port}\n")
            elif key == "gameservlist":
                # Allow our mock D2GS (localhost) to connect.
                out.append("gameservlist = 127.0.0.1\n")
            elif key == "charsavedir":
                out.append(f'charsavedir = "{var}/charsave"\n')
            elif key == "charinfodir":
                out.append(f'charinfodir = "{var}/charinfo"\n')
            elif key == "ladderdir":
                out.append(f'ladderdir = "{var}/ladders"\n')
            elif key == "bak_charsavedir":
                out.append(f'bak_charsavedir = "{var}/bak/charsave"\n')
            elif key == "bak_charinfodir":
                out.append(f'bak_charinfodir = "{var}/bak/charinfo"\n')
            elif key == "logfile":
                out.append(f'logfile = "{var}/d2dbs.log"\n')
            elif key == "logfile-gs":
                out.append(f'logfile-gs = "{var}/d2dbs-gs.log"\n')
            else:
                out.append(line)
        with open(conf_path, "w") as f:
            f.writelines(out)

    def start(self, timeout: float = 15.0) -> None:
        self.setup()
        conf = os.path.join(self.home, "etc", "d2dbs.conf")
        self._logf = open(os.path.join(self.home, "var", "d2dbs.out"), "a+")
        self.proc = subprocess.Popen(
            [self._bin(), "-f", "-c", conf],
            stdout=self._logf, stderr=subprocess.STDOUT,
            cwd=os.path.join(self.home, "etc"))
        _await_listen(self.proc, self.port, self._logf, "original d2dbs", timeout)

    def stop(self) -> None:
        _stop(self.proc)
        if self._logf:
            self._logf.close()
        if self.home and os.path.isdir(self.home):
            shutil.rmtree(self.home, ignore_errors=True)

    def __enter__(self):
        self.start(); return self

    def __exit__(self, *a):
        self.stop()


def _await_listen(proc, port, logf, label, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if proc.poll() is not None:
            logf.seek(0)
            raise RuntimeError(
                f"{label} exited early (rc={proc.returncode}):\n{logf.read()[-3000:]}")
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5):
                return
        except OSError:
            time.sleep(0.1)
    raise TimeoutError(f"{label} did not start listening on {port}")


def _stop(proc):
    if proc and proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
