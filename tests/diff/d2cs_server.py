# SPDX-License-Identifier: GPL-2.0-or-later
"""Launchers for the D2CS (Diablo II Character Server) under differential test.

`V3D2cs`        — the v3 rewrite's standalone d2cs (in-memory, stubbed auth).
`OriginalD2cs`  — the original pvpgn d2cs binary. NOTE: the original d2cs
                  REQUIRES a running bnetd to authenticate a client LOGINREQ
                  (handle_d2cs.cpp rejects login when bnetd_conn() is null), so
                  it can be started standalone to probe the init handshake /
                  pre-auth behaviour, but the character flow needs a bnetd peer
                  (a later harness extension).
"""
import os
import shutil
import socket
import subprocess
import tempfile
import time


class V3D2cs:
    def __init__(self, d2cs_bin: str, port: int):
        self.bin = d2cs_bin
        self.port = port
        self.proc = None
        self.workdir = None
        self.logf = None

    def start(self, timeout: float = 15.0) -> None:
        self.workdir = tempfile.mkdtemp(prefix="pvpgn-v3d2cs-")
        self.logf = open(os.path.join(self.workdir, "d2cs.log"), "a+")
        self.proc = subprocess.Popen(
            [self.bin, "--port", str(self.port),
             "--listen", "127.0.0.1", "--log-level", "info"],
            stdout=self.logf, stderr=subprocess.STDOUT)
        self._await(timeout)

    def _await(self, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                self.logf.seek(0)
                raise RuntimeError(
                    f"v3 d2cs exited early (rc={self.proc.returncode}):\n{self.logf.read()}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5):
                    return
            except OSError:
                time.sleep(0.1)
        raise TimeoutError("v3 d2cs did not start listening")

    def stop(self) -> None:
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        if self.logf:
            self.logf.close()
        if self.workdir and os.path.isdir(self.workdir):
            shutil.rmtree(self.workdir, ignore_errors=True)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *a):
        self.stop()


class OriginalD2cs:
    """Start the original pvpgn d2cs in an isolated temp home.

    `bnetd_port` points the d2cs<->bnetd link at a running bnetd; pass None to
    start standalone (login will fail without a bnetd, but the listener and init
    handshake can still be probed).
    """
    def __init__(self, repo: str, port: int, bnetd_port=None):
        self.repo = repo
        self.port = port
        self.bnetd_port = bnetd_port
        self.proc = None
        self.home = None

    def _bin(self) -> str:
        cand = os.path.join(self.repo, "build", "src", "d2cs", "d2cs")
        if not os.path.isfile(cand):
            raise FileNotFoundError(f"original d2cs binary not found: {cand}")
        return cand

    def _conf_src(self) -> str:
        return os.path.join(self.repo, "build", "conf")

    def setup(self) -> None:
        self.home = tempfile.mkdtemp(prefix="pvpgn-origd2cs-")
        etc = os.path.join(self.home, "etc")
        var = os.path.join(self.home, "var")
        os.makedirs(etc, exist_ok=True)
        for sub in ("charsave", "charinfo", "ladders", "bak"):
            os.makedirs(os.path.join(var, sub), exist_ok=True)
        src = self._conf_src()
        for name in os.listdir(src):
            s = os.path.join(src, name)
            if os.path.isfile(s):
                shutil.copy(s, os.path.join(etc, name))
        self._rewrite(os.path.join(etc, "d2cs.conf"), etc, var)

    def _rewrite(self, conf_path: str, etc: str, var: str) -> None:
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
            elif key == "bnetdaddr" and self.bnetd_port:
                out.append(f"bnetdaddr = 127.0.0.1:{self.bnetd_port}\n")
            elif key == "charsavedir":
                out.append(f'charsavedir = "{var}/charsave"\n')
            elif key == "charinfodir":
                out.append(f'charinfodir = "{var}/charinfo"\n')
            elif key == "ladderdir":
                out.append(f'ladderdir = "{var}/ladders"\n')
            elif key == "bak_charsave_dir":
                out.append(f'bak_charsave_dir = "{var}/bak"\n')
            elif key == "logfile":
                out.append(f'logfile = "{var}/d2cs.log"\n')
            else:
                out.append(line)
        with open(conf_path, "w") as f:
            f.writelines(out)

    def start(self, timeout: float = 15.0) -> None:
        self.setup()
        conf = os.path.join(self.home, "etc", "d2cs.conf")
        self.proc = subprocess.Popen(
            [self._bin(), "-f", "-c", conf],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=os.path.join(self.home, "etc"))
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                out = self.proc.stdout.read().decode("utf-8", "replace")
                raise RuntimeError(
                    f"original d2cs exited early (rc={self.proc.returncode}):\n{out[-3000:]}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5):
                    return
            except OSError:
                time.sleep(0.1)
        raise TimeoutError("original d2cs did not start listening")

    def stop(self) -> None:
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
        if self.home and os.path.isdir(self.home):
            shutil.rmtree(self.home, ignore_errors=True)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *a):
        self.stop()
