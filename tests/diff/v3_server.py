# SPDX-License-Identifier: GPL-2.0-or-later
"""Launch the v3 bnetd for differential testing (mirrors the e2e harness)."""
import os
import socket
import subprocess
import tempfile
import time


class V3Bnetd:
    def __init__(self, bnetd_bin: str, port: int):
        self.bin = bnetd_bin
        self.port = port
        self.proc = None
        self.workdir = None
        self.logf = None

    def start(self, timeout: float = 15.0) -> None:
        self.workdir = tempfile.mkdtemp(prefix="pvpgn-v3diff-")
        cfg = os.path.join(self.workdir, "bnetd.toml")
        with open(cfg, "w") as f:
            f.write('[persistence]\nbackend = "inmemory"\n')
        self.logf = open(os.path.join(self.workdir, "bnetd.log"), "a+")
        self.proc = subprocess.Popen(
            [self.bin, "-c", cfg, "-p", str(self.port), "-d", self.workdir, "-l", "info"],
            stdout=self.logf, stderr=subprocess.STDOUT)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                self.logf.seek(0)
                raise RuntimeError(
                    f"v3 bnetd exited early (rc={self.proc.returncode}):\n{self.logf.read()}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5):
                    return
            except OSError:
                time.sleep(0.1)
        raise TimeoutError("v3 bnetd did not start listening")

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
            import shutil
            shutil.rmtree(self.workdir, ignore_errors=True)

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *a):
        self.stop()
