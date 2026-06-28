#!/usr/bin/env python3
"""Raw-IRC registration handshake: NICK+USER -> welcome numerics (001..004).

Drives the original's IRC listener (OriginalBnetd, ircaddrs on orig_port+4) and
v3's dedicated IRC listener (V3Bnetd.irc_port). Both must complete the IRC
registration with the RPL_WELCOME (001) burst. This is the foundation diff for
non-WOL IRC coverage (a previously-untested protocol in the harness).
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
from irc_client import IrcClient


# The client-critical registration burst both servers must emit. The oracle also
# sends 372 (a MOTD content line from its configured MOTD file); v3 has no MOTD
# content, so 372 is excluded from the required set.
REQUIRED = {1, 2, 3, 4, 5, 375, 376}


def probe(host, port):
    c = IrcClient(host, port)
    lines = c.register("ircreg")
    c.close()
    nums = set(IrcClient.numerics(lines))
    return {"has_burst": REQUIRED.issubset(nums), "nums": sorted(nums)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6700)
    ap.add_argument("--v3-port", type=int, default=6720)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", orig.irc_port)
        print(f"oracle: {o}")
        if a.orig_only:
            return 0 if o["has_burst"] else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", v3.irc_port)
            print(f"v3    : {n}")
            ok = o["has_burst"] and n["has_burst"]
            print("OK: IRC registration emits the full registration burst (001-005,375,376) on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
