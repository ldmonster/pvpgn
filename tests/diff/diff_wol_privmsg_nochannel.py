#!/usr/bin/env python3
"""Differential guard: WOL PRIVMSG to a channel while NOT in any channel.

The original WOL _handle_privmsg_command (handle_wol.cpp:402-423) keys solely on
conn_get_channel(conn): a client that is on no channel ALWAYS gets
403 ERR_NOSUCHCHANNEL with the channel as a MIDDLE param
(":<server> 403 <nick> #Chat :No such channel"), never 404 ERR_CANNOTSENDTOCHAN.
v3 previously replied 404 with the channel shoved into the trailing param behind
a stray ':'. This guard asserts both servers emit numeric 403 with the channel
as a middle parameter (server name + text normalized).
"""
import os, sys, time
DIFF = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, DIFF)
from original_server import OriginalBnetd
from v3_server import V3Bnetd
from wol_client import WolClient, apgar_for

PORT_BASE = 13480  # oracle BNCS; WOL = +2
V3_BASE = 13486


def raw_login(host, port, user, pw="pw"):
    c = WolClient(host, port)
    c.send_line("CVERS 1 1000")
    c.send_line("VERCHK 1000 1.0")
    c.send_line(f"APGAR {apgar_for(pw)}")
    c.send_line(f"NICK {user}")
    c.send_line(f"USER {user} HostName irc.westwood.com :RN")
    for _ in range(60):
        line = c.read_line()
        if line is None:
            break
        code = WolClient.numeric(line)
        if code == 376:
            return c
        if code in (378, 433):
            break
    c.close()
    return None


def privmsg_nochannel(host, port, user):
    c = raw_login(host, port, user)
    assert c is not None, f"login failed on {host}:{port}"
    c.sock.sendall(b"PRIVMSG #Chat :hi\r\n")
    c.sock.settimeout(1.0)
    line = c.read_line()
    c.close()
    return line


def parse(line):
    """Return (numeric, [middle params after nick], trailing) normalizing server name/nick."""
    assert line is not None, "no reply received"
    assert line.startswith(":"), f"no prefix: {line!r}"
    # strip ":server " prefix
    rest = line[1:].split(" ", 1)[1]
    # split off trailing
    if " :" in rest:
        head, trailing = rest.split(" :", 1)
    else:
        head, trailing = rest, None
    toks = head.split()
    numeric = toks[0]
    # toks[1] is the nick; middles are toks[2:]
    middles = toks[2:]
    return numeric, middles, trailing


def main():
    orig = OriginalBnetd("/home/cnupt/work/pvpgn-server", PORT_BASE)
    v3 = V3Bnetd("/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd", V3_BASE)
    try:
        orig.start()
        v3.start()
        time.sleep(1.0)
        o = privmsg_nochannel("127.0.0.1", orig.wolv1_port, "ora")
        v = privmsg_nochannel("127.0.0.1", v3.wol_port, "v3u")
        print("ORACLE:", repr(o))
        print("V3    :", repr(v))
        on, om, ot = parse(o)
        vn, vm, vt = parse(v)
        assert on == "403", f"oracle numeric expected 403, got {on}"
        assert vn == "403", f"v3 numeric expected 403, got {vn} (line {v!r})"
        assert om == ["#Chat"], f"oracle channel not a middle param: {om}"
        assert vm == ["#Chat"], f"v3 channel not a middle param: {vm} (line {v!r})"
        # trailing must NOT contain the channel (no stray-colon shoving)
        assert vt and "#Chat" not in vt, f"v3 stray-colon channel in trailing: {vt!r}"
        print("PASS: both servers emit 403 with #Chat as a middle param")
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    main()
