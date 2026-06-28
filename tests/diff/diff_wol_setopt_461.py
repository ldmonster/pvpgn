#!/usr/bin/env python3
"""WOL SETOPT with no parameter -> 461 ERR_NEEDMOREPARAMS.

The original handle_wol.cpp sends 461 when SETOPT is called with numparams < 1;
v3 silently returned with no reply. Both must now send a 461.
"""
import argparse, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import wol_client as wc


def _drain(c, settle=0.6):
    time.sleep(settle)
    c.sock.settimeout(0.5)
    out = []
    try:
        while True:
            line = c.read_line()
            if line is None:
                break
            out.append(line)
    except OSError:
        pass
    return out


def saw_461(host, port):
    c = wc.wol_session(host, port, "optu", "pw")
    if c is None:
        return None
    time.sleep(0.2)
    _drain(c)
    c.send_line("SETOPT")
    lines = _drain(c)
    c.close()
    return any(len(l.split()) >= 2 and l.split()[1] == "461" for l in lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6426)
    ap.add_argument("--v3-port", type=int, default=6526)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = saw_461("127.0.0.1", orig.wolv1_port)
        n = saw_461("127.0.0.1", v3.wol_port)
        print(f"bare SETOPT -> 461: oracle={o} v3={n}")
        ok = (o is True and n is True)
        print("OK: both send 461 for paramless SETOPT" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
