#!/usr/bin/env python3
"""/away and /dnd reply EID_INFO (0x12), not the unknown-command EID_ERROR.

The original _handle_away_command / _handle_dnd_command acknowledge with
message_type_info (EID_INFO 0x12), toggling the away/DND state. v3 previously
lacked these commands so they fell through to EID_ERROR (0x13) "Unknown command".
Compares the reply EID kind (the localized text is charset-garbled in the harness).
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

EID_INFO = 0x12


def first_eid(events):
    return events[0][0] if events else None


def probe(host, port, who):
    c, _ = bc.full_login(host, port, who, "pw")
    out = {
        "away_on":  first_eid(bc.chat_command(c, "/away")),
        "away_off": first_eid(bc.chat_command(c, "/away")),       # toggle back
        "dnd_on":   first_eid(bc.chat_command(c, "/dnd")),
        "dnd_off":  first_eid(bc.chat_command(c, "/dnd")),
    }
    c.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6428)
    ap.add_argument("--v3-port", type=int, default=6528)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = probe("127.0.0.1", a.orig_port, "awayo")
        n = probe("127.0.0.1", a.v3_port, "awayn")
        print(f"oracle EIDs: {o}")
        print(f"v3     EIDs: {n}")
        ok = (all(v == EID_INFO for v in o.values()) and
              all(v == EID_INFO for v in n.values()))
        print("OK: /away and /dnd reply EID_INFO on both (toggle)" if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
