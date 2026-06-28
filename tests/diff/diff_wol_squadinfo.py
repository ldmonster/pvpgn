#!/usr/bin/env python3
"""WOL SQUADINFO/CLANBYNAME 439 ERR_IDNOEXIST format.

For a clanless account the original answers SQUADINFO with
':<server> 439 <nick> :ID does not exist' — the command name is NOT echoed in
the reply. v3 previously included it (':... 439 nick SQUADINFO :ID does not
exist'). Compares the parsed 439 trailing/middle.
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


def squadinfo_439(host, port):
    c = wc.wol_session(host, port, "sq", "pw")
    if c is None:
        return None
    time.sleep(0.2); _drain(c)
    c.send_line("SQUADINFO test")
    line439 = None
    for l in _drain(c):
        p = l.split()
        if len(p) >= 2 and p[1] == "439":
            line439 = l
            break
    c.close()
    if line439 is None:
        return None
    # tokens after the nick (everything past 'NNN <nick>'); the command name, if
    # wrongly present, appears as a middle param before the ':' trailing.
    colon = line439.find(" :")
    middle = line439[:colon].split()[3:] if colon >= 0 else []
    trailing = line439[colon + 2:] if colon >= 0 else ""
    return {"has_cmd_in_middle": "SQUADINFO" in middle, "trailing": trailing}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6430)
    ap.add_argument("--v3-port", type=int, default=6530)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = squadinfo_439("127.0.0.1", orig.wolv1_port)
        n = squadinfo_439("127.0.0.1", v3.wol_port)
        print(f"oracle: {o}")
        print(f"v3    : {n}")
        ok = (o and n and o["has_cmd_in_middle"] is False
              and n["has_cmd_in_middle"] is False
              and o["trailing"] == n["trailing"])
        print("OK: 439 format matches oracle (no command name, same trailing)"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
