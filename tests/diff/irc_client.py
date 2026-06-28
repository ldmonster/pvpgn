#!/usr/bin/env python3
"""Minimal raw-IRC mock client for the differential harness.

Drives the NON-WOL IRC dialect: the original's IRC listener (enabled via
ircaddrs in OriginalBnetd, port = orig_port+4) and v3's dedicated IRC listener
(V3Bnetd.irc_port = v3_port+3). Connects, registers (NICK+USER), and exchanges
line-based IRC commands, parsing numeric replies.
"""
import socket
import time


class IrcClient:
    def __init__(self, host, port, timeout=10.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(1.5)
        self.buf = b""

    def send_line(self, line):
        if isinstance(line, str):
            line = line.encode("latin-1", "replace")
        self.sock.sendall(line + b"\r\n")

    def recv_lines(self, settle=0.5, max_wait=3.0):
        """Collect complete IRC lines for a short window (answers PING)."""
        time.sleep(settle)
        lines = []
        end = time.time() + max_wait
        while time.time() < end:
            try:
                d = self.sock.recv(4096)
            except socket.timeout:
                break
            except OSError:
                break
            if not d:
                break
            self.buf += d
            while b"\r\n" in self.buf:
                raw, self.buf = self.buf.split(b"\r\n", 1)
                ln = raw.decode("latin-1", "replace")
                # Answer server PING so the session stays alive.
                if ln.upper().startswith("PING"):
                    token = ln[4:].strip()
                    self.send_line("PONG " + token)
                    continue
                lines.append(ln)
        return lines

    def register(self, nick, user="u", realname="Real Name"):
        self.send_line(f"NICK {nick}")
        self.send_line(f"USER {user} 0 * :{realname}")
        return self.recv_lines()

    def command(self, cmd):
        self.send_line(cmd)
        return self.recv_lines()

    @staticmethod
    def numerics(lines):
        """Parse ':<server> NNN <nick> <rest>' -> list of int reply codes."""
        out = []
        for ln in lines:
            parts = ln.split(" ", 3)
            if len(parts) >= 2 and parts[0].startswith(":") and parts[1].isdigit():
                out.append(int(parts[1]))
        return out

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass
