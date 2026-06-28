#!/usr/bin/env python3
"""SID_CLANINFO (0x82): server replies CLANINFOREPLY for an existing account.

The original _client_claninforeq drops the packet if the queried account does
not exist, otherwise ALWAYS replies SERVER_CLANINFOREPLY (fail=0 with clan data
if the account is in the requested clan, else fail=1). v3 had a no-op stub that
hung the client. Neither server gives a freshly-created account a clan, so both
reply fail=1 (cookie echoed) when the client queries its own name.
"""
import argparse, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

SID_CLANINFO = 0x82


def _query(c, name, clantag, cookie):
    body = struct.pack("<II", cookie, clantag) + bc.cstring(name)
    c.send(SID_CLANINFO, body)
    rep = bc._drain_until(c, SID_CLANINFO)
    if rep is None or len(rep) < 5:
        return None
    return (struct.unpack_from("<I", rep, 0)[0], rep[4])


def claninfo(host, port, name):
    c, _ = bc.full_login(host, port, name, "pw")
    # tag 0 matches a clanless account -> fail=0; a non-zero tag -> fail=1.
    match = _query(c, name, 0, 0x1234)
    nomatch = _query(c, name, 0x41424344, 0x5678)
    c.close()
    return {"match": match, "nomatch": nomatch}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6424)
    ap.add_argument("--v3-port", type=int, default=6524)
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
    try:
        orig.start(); v3.start()
        o = claninfo("127.0.0.1", a.orig_port, "claner")
        n = claninfo("127.0.0.1", a.v3_port, "claner")
        print(f"CLANINFOREPLY oracle={o!r}")
        print(f"CLANINFOREPLY v3    ={n!r}")
        ok = (o == n and
              o["match"] == (0x1234, 0) and       # tag 0 == clanless -> fail 0
              o["nomatch"] == (0x5678, 1))        # non-zero tag -> fail 1
        print("OK: CLANINFOREPLY matches oracle (cookie echoed, tag-match fail)"
              if ok else "FAIL")
        return 0 if ok else 1
    finally:
        v3.stop(); orig.stop()


sys.exit(main())
