# SPDX-License-Identifier: GPL-2.0-or-later
"""Launch the ORIGINAL pvpgn-server bnetd in an isolated temp home.

Differential-testing reference: the upstream server is the known-good oracle.
This sets up a throwaway pvpgn home (conf files copied from the build tree, all
paths rewritten into the temp dir, a test port), starts `bnetd -f`, and waits
for it to accept connections. Same lifecycle shape as the v3 e2e harness.
"""
import os
import re
import shutil
import socket
import subprocess
import tempfile
import time


class OriginalBnetd:
    def __init__(self, repo: str, port: int):
        self.repo = repo
        self.port = port
        self.proc = None
        self.home = None

    def _conf_src(self) -> str:
        cand = os.path.join(self.repo, "build", "conf")
        if not os.path.isdir(cand):
            raise FileNotFoundError(f"original conf dir not found: {cand}")
        return cand

    def _bnetd_bin(self) -> str:
        cand = os.path.join(self.repo, "build", "src", "bnetd", "bnetd")
        if not os.path.isfile(cand):
            raise FileNotFoundError(f"original bnetd binary not found: {cand}")
        return cand

    def setup(self) -> None:
        self.home = tempfile.mkdtemp(prefix="pvpgn-orig-")
        etc = os.path.join(self.home, "etc")
        var = os.path.join(self.home, "var")
        os.makedirs(etc, exist_ok=True)
        for sub in ("users", "clans", "teams", "files"):
            os.makedirs(os.path.join(var, sub), exist_ok=True)

        # Copy every generated conf file into our private etc/.
        src = self._conf_src()
        for name in os.listdir(src):
            s = os.path.join(src, name)
            if os.path.isfile(s):
                shutil.copy(s, os.path.join(etc, name))
        # i18n localization dir (common.xml etc.) if present.
        i18n = os.path.join(src, "i18n")
        if os.path.isdir(i18n):
            shutil.copytree(i18n, os.path.join(etc, "i18n"), dirs_exist_ok=True)

        self._rewrite_conf(os.path.join(etc, "bnetd.conf"), etc, var)
        self._create_support_files(os.path.join(var, "files"), etc)

    def _create_support_files(self, files_dir: str, etc: str) -> None:
        # bnetd's support_check_files aborts startup if any file listed in
        # supportfile.conf is missing from filedir. The check is existence-only,
        # so empty placeholders satisfy it (we never drive CheckRevision here).
        sf = os.path.join(etc, "supportfile.conf")
        names = []
        if os.path.isfile(sf):
            with open(sf) as f:
                for line in f:
                    s = line.strip()
                    if s and not s.startswith("#"):
                        names.append(s)
        for name in names:
            p = os.path.join(files_dir, os.path.basename(name))
            if not os.path.exists(p):
                open(p, "wb").close()

    def _rewrite_conf(self, conf_path: str, etc: str, var: str) -> None:
        with open(conf_path, "r") as f:
            lines = f.readlines()

        out = []
        for line in lines:
            stripped = line.strip()
            if stripped.startswith("#") or "=" not in stripped:
                out.append(line)
                continue
            key = stripped.split("=", 1)[0].strip()
            if key == "storage_path":
                out.append(
                    'storage_path = "file:mode=plain;dir={v}/users;'
                    'clan={v}/clans;team={v}/teams;'
                    'default={e}/bnetd_default_user.plain"\n'.format(v=var, e=etc))
            elif key == "filedir":
                out.append(f'filedir = "{var}/files"\n')
            elif key == "logfile":
                out.append(f'logfile = "{var}/bnetd.log"\n')
            elif key == "servaddrs":
                out.append(f'servaddrs = 127.0.0.1:{self.port}\n')
            elif key in ("w3routeaddr",):
                # keep w3route off a real port; bind to an ephemeral local one
                out.append(f'w3routeaddr = 127.0.0.1:{self.port + 1}\n')
            elif key.endswith("file") or key.endswith("_file") or key in (
                    "channelfile", "adfile", "topicfile", "ipbanfile",
                    "mpqfile", "realmfile", "mapsfile", "xplevelfile",
                    "xpcalcfile", "aliasfile", "supportfile", "transfile",
                    "issuefile", "DBlayoutfile"):
                # Rewrite any absolute /usr/local/etc/pvpgn/<name> reference to
                # our private etc/. Bare filenames (motd, icons.bni) are left as
                # relative; bnetd resolves those against filedir and tolerates
                # their absence with a warning.
                m = re.match(r'^(\w+)\s*=\s*"?([^"#]*?)"?\s*(#.*)?$', stripped)
                if m:
                    val = m.group(2).strip()
                    base = os.path.basename(val)
                    if val.startswith("/usr/local/etc/pvpgn/") or \
                       val.startswith("/usr/local/var/pvpgn/"):
                        out.append(f'{key} = "{os.path.join(etc, base)}"\n')
                    else:
                        out.append(line)
                else:
                    out.append(line)
            else:
                out.append(line)
        with open(conf_path, "w") as f:
            f.writelines(out)

    def start(self, timeout: float = 15.0) -> None:
        self.setup()
        conf = os.path.join(self.home, "etc", "bnetd.conf")
        self.proc = subprocess.Popen(
            [self._bnetd_bin(), "-f", "-c", conf],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd=os.path.join(self.home, "etc"))
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                out = self.proc.stdout.read().decode("utf-8", "replace")
                raise RuntimeError(
                    f"original bnetd exited early (rc={self.proc.returncode}):\n{out[-3000:]}")
            try:
                with socket.create_connection(("127.0.0.1", self.port), timeout=0.5):
                    return
            except OSError:
                time.sleep(0.1)
        raise TimeoutError("original bnetd did not start listening in time")

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


if __name__ == "__main__":
    import sys
    repo = sys.argv[1] if len(sys.argv) > 1 else "/home/cnupt/work/pvpgn-server"
    srv = OriginalBnetd(repo, port=6112)
    try:
        srv.start()
        print(f"[ok] original bnetd listening on 127.0.0.1:{srv.port}")
    finally:
        srv.stop()
