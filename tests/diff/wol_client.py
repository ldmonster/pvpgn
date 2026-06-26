# SPDX-License-Identifier: GPL-2.0-or-later
"""A protocol-faithful Westwood Online (WOL) mock client for differential testing.

WOL is an IRC dialect spoken on a dedicated listener (wolv1addrs / wolv2addrs),
NOT the BNCS port. The login handshake the original pvpgn-server expects
(src/bnetd/handle_wol.cpp) is:

    -> CVERS <oldvernum> <SKU>     server maps SKU -> clienttag (tag_sku_to_uint)
    -> VERCHK <SKU> <version>      <- :host 379 user :none none none 1 SKU NONREQ
    -> APGAR <passhash>            server stores the password hash (opaque)
    -> NICK  <username>            sets the logged-in user name
    -> USER  <user> <host> irc.westwood.com :<realname>
                                   triggers welcome: on success the MOTD
                                   (375 .. 372 .. 376); on a bad password 378
                                   (RPL_BAD_LOGIN); if already logged in 433.

The server AUTO-CREATES the account on first login, storing the APGAR verbatim,
and on later logins compares the stored APGAR against the one sent (plain string
compare — it does not re-derive the Westwood hash). So a deterministic,
password-derived token is a faithful stand-in: it is stable across both servers
and differs for a wrong password, exactly as a real APGAR would behave here.

Lines are CRLF-terminated; server replies are IRC numerics of the form
``:<servername> <code> <nick> <params>``.
"""
import base64
import hashlib
import socket

# IRC / WOL numeric replies we care about.
RPL_WELCOME       = 1
RPL_MOTD          = 372
RPL_MOTDSTART     = 375
RPL_ENDOFMOTD     = 376
RPL_BAD_LOGIN     = 378   # wrong APGAR (password)
RPL_VERCHK_NONREQ = 379
ERR_NICKNAMEINUSE = 433


def apgar_for(password: str) -> str:
    """A deterministic stand-in for the Westwood APGAR password token. The
    original stores/compares it opaquely, so any stable, password-derived string
    works; we use base64(sha1(password)) trimmed of '=' padding (APGAR tokens are
    base64-ish and contain no spaces, which keeps the IRC tokenizer happy)."""
    digest = hashlib.sha1(password.encode("utf-8")).digest()
    return base64.b64encode(digest).decode("ascii").rstrip("=")


class WolClient:
    def __init__(self, host: str, port: int, timeout: float = 5.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.settimeout(timeout)
        self.buf = b""

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass

    def send_line(self, line: str):
        self.sock.sendall(line.encode("latin-1") + b"\r\n")

    def read_line(self):
        """Return the next CRLF line as str, or None on close/timeout."""
        while b"\r\n" not in self.buf:
            try:
                chunk = self.sock.recv(4096)
            except (socket.timeout, OSError):
                return None
            if not chunk:
                return None
            self.buf += chunk
        line, self.buf = self.buf.split(b"\r\n", 1)
        return line.decode("latin-1", "replace")

    @staticmethod
    def numeric(line: str):
        """Extract the IRC numeric code from a ':host CODE nick ...' line."""
        if not line.startswith(":"):
            return None
        parts = line.split(" ", 2)
        if len(parts) < 2:
            return None
        try:
            return int(parts[1])
        except ValueError:
            return None


def wol_login(host, port, username, password, sku=1000, oldver="1",
              version="1.0", realname="RealName"):
    """Drive the full WOL login handshake. Returns a normalized dict:
       {ok, code, verchk, lines}. ok=True iff the server completed the MOTD
       welcome (login accepted / account auto-created)."""
    c = WolClient(host, port)
    out = {"verchk": None, "lines": [], "code": None, "ok": False}
    try:
        c.send_line(f"CVERS {oldver} {sku}")
        c.send_line(f"VERCHK {sku} {version}")
        c.send_line(f"APGAR {apgar_for(password)}")
        c.send_line(f"NICK {username}")
        c.send_line(
            f"USER {username} HostName irc.westwood.com :{realname}")

        # Read replies until the welcome MOTD finishes or login is refused.
        for _ in range(60):
            line = c.read_line()
            if line is None:
                break
            out["lines"].append(line)
            code = WolClient.numeric(line)
            if code == RPL_VERCHK_NONREQ:
                out["verchk"] = line
            elif code == RPL_ENDOFMOTD:
                out["code"] = RPL_ENDOFMOTD
                out["ok"] = True
                break
            elif code in (RPL_BAD_LOGIN, ERR_NICKNAMEINUSE):
                out["code"] = code
                out["ok"] = False
                break
        return out
    finally:
        c.close()


RPL_CHANNEL  = 327
RPL_LISTEND  = 323
RPL_ENDOFNAMES = 366


def wol_session(host, port, username, password, sku=1000, oldver="1",
                version="1.0", realname="RealName"):
    """Do the WOL login handshake and return the OPEN, logged-in WolClient (for
    post-login LIST/JOIN). Returns None if login was refused."""
    c = WolClient(host, port)
    c.send_line(f"CVERS {oldver} {sku}")
    c.send_line(f"VERCHK {sku} {version}")
    c.send_line(f"APGAR {apgar_for(password)}")
    c.send_line(f"NICK {username}")
    c.send_line(f"USER {username} HostName irc.westwood.com :{realname}")
    for _ in range(60):
        line = c.read_line()
        if line is None:
            break
        code = WolClient.numeric(line)
        if code == RPL_ENDOFMOTD:
            return c
        if code in (RPL_BAD_LOGIN, ERR_NICKNAMEINUSE):
            break
    c.close()
    return None


def wol_join(client, channel):
    """JOIN a channel; read until the NAMES list ends (366) or a few lines."""
    client.send_line(f"JOIN {channel}")
    lines = []
    for _ in range(20):
        line = client.read_line()
        if line is None:
            break
        lines.append(line)
        if WolClient.numeric(line) == RPL_ENDOFNAMES:
            break
        # The original echoes the JOIN (":nick!... JOIN ...") and a 353 NAMES.
        if " JOIN " in line and len(lines) >= 2:
            break
    return lines


def wol_list(client):
    """LIST; collect RPL_CHANNEL (327) entries until RPL_LISTEND (323). Returns
    the channel names (first token of each 327 reply, '#'-normalized)."""
    client.send_line("LIST")
    names = []
    for _ in range(80):
        line = client.read_line()
        if line is None:
            break
        code = WolClient.numeric(line)
        if code == RPL_LISTEND:
            break
        if code == RPL_CHANNEL:
            # ":server 327 nick <name> <count> <flag> 388"
            parts = line.split()
            if len(parts) >= 4:
                name = parts[3].lstrip("#").lower()
                names.append(name)
    return sorted(names)


def wol_privmsg(client, target, message):
    """Send a PRIVMSG <target> :<message> (no reply expected for the sender)."""
    client.send_line(f"PRIVMSG {target} :{message}")


def wol_joingame_create(client, name, minp, maxp, gtype, tournament=0):
    """CREATE a WOL game-channel (WOLv1: 7 params)."""
    client.send_line(f"JOINGAME {name} {minp} {maxp} {gtype} 1 1 {tournament}")


def wol_joingame_join(client, name, password=None):
    """JOIN an existing WOL game-channel (2 params, or 3 with a password)."""
    if password is None:
        client.send_line(f"JOINGAME {name} 1")
    else:
        client.send_line(f"JOINGAME {name} 1 {password}")


def wol_read_after_verb(client, verb, tries=20):
    """Read until a ":<src>!.. <VERB> <rest>" line; return (sender, rest) or None.
    `rest` is the full payload after the verb (for JOINGAME acks etc.)."""
    tok = f" {verb} "
    for _ in range(tries):
        line = client.read_line()
        if line is None:
            break
        if tok not in line:
            continue
        prefix, _, after = line.partition(tok)
        sender = prefix[1:].split("!", 1)[0] if prefix.startswith(":") else prefix
        return (sender, after)
    return None


def wol_gameopt(client, target, options):
    """Send GAMEOPT <target> :<options> (channel '#...' or a nick)."""
    client.send_line(f"GAMEOPT {target} :{options}")


def wol_read_verb(client, verb, tries=20):
    """Read lines until one of the form ":<src>!.. <VERB> <target> :<text>" is
    seen, returning (sender, target, text), or None. Generic over PRIVMSG,
    GAMEOPT, STARTG, etc."""
    tok = f" {verb} "
    for _ in range(tries):
        line = client.read_line()
        if line is None:
            break
        if tok not in line:
            continue
        prefix, _, after = line.partition(tok)
        sender = prefix[1:].split("!", 1)[0] if prefix.startswith(":") else prefix
        target, _, text = after.partition(" ")
        if text.startswith(":"):
            text = text[1:]
        return (sender, target, text)
    return None


def wol_read_privmsg(client, want_channel=None, tries=20):
    """Read lines until a PRIVMSG is received, returning (sender, target, text)
    or None if none arrives. Filters to want_channel when given."""
    for _ in range(tries):
        line = client.read_line()
        if line is None:
            break
        # ":<nick>!<user>@<host> PRIVMSG <target> :<text>"
        if " PRIVMSG " not in line:
            continue
        prefix, _, after = line.partition(" PRIVMSG ")
        sender = prefix[1:].split("!", 1)[0] if prefix.startswith(":") else prefix
        target, _, text = after.partition(" ")
        if text.startswith(":"):
            text = text[1:]
        if want_channel is not None and target.lstrip("#").lower() != \
                want_channel.lstrip("#").lower():
            continue
        return (sender, target, text)
    return None


if __name__ == "__main__":
    import sys
    h = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    p = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
    print(wol_login(h, p, "wolprobe", "secretpass", sku=1000))
