#!/usr/bin/env python3
"""DIFFERENTIAL D2CS CHARLISTREPLY byte parity with a real character.

The newbie-template unblock (wave 220) lets the oracle create a real character,
so the CHARLISTREPLY (0x17) — including v3's hand-built 33-byte portrait block —
is now byte-comparable against the original for the first time.

Both servers log in, create the SAME character (class 4 / Barbarian, expansion
status 0x20), then issue CHARLISTREQ. The full reply body must be byte-identical:
header counts + name + the portrait block (header, gfx, chclass=class+1, color,
level, status, ladder, ...).

This caught a real divergence (wave 227): the portrait STATUS byte was 0xA0 on v3
vs 0xA1 on the oracle — v3 omitted the INIT status bit (0x01) that the original's
d2char_create sets on creation (charstatus_set_init). With INIT set, the bodies
match exactly.
"""
import argparse
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from d2cs_server import OriginalD2cs, V3D2cs  # noqa: E402
import bncs_client as bc  # noqa: E402
import d2cs_client as dc  # noqa: E402


def charlist_raw(c):
    c.send(0x17, struct.pack("<HH", 8, 0))  # CHARLISTREQ: maxchar + u1
    return c.recv_type(0x17)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-d2cs", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/d2cs/pvpgn_v3_d2cs"))
    ap.add_argument("--bnetd-port", type=int, default=9060)
    ap.add_argument("--orig-d2cs-port", type=int, default=9080)
    ap.add_argument("--v3-d2cs-port", type=int, default=9100)
    a = ap.parse_args()

    bnetd = OriginalBnetd(a.orig_repo, a.bnetd_port,
                          realm={"name": "test", "d2cs_port": a.orig_d2cs_port})
    od = OriginalD2cs(a.orig_repo, a.orig_d2cs_port,
                      bnetd_port=a.bnetd_port, realm_name="test")
    v3 = V3D2cs(a.v3_d2cs, a.v3_d2cs_port)
    try:
        bnetd.start(); od.start(); v3.start()
        time.sleep(1.5)

        cli, _ = bc.full_login("127.0.0.1", a.bnetd_port, "clb", "pw", product=b"D2DV")
        rj = bc.realm_join(cli, "test", seqno=1)
        oc = dc.D2csClient("127.0.0.1", a.orig_d2cs_port)
        oc.login("clb", sessionnum=rj["sessionnum"], sessionkey=rj["sessionkey"],
                 secret_hash_raw=rj["secret_hash"], seqno=1)
        oc.create_char("Conan", char_class=4, status=0x20)
        o = charlist_raw(oc)
        oc.close(); cli.close()

        vc = dc.D2csClient("127.0.0.1", a.v3_d2cs_port)
        vc.login("clb", sessionnum=1,
                 secret_hash_raw=dc.d2cs_token("clb", 1, 7), seqno=7)
        vc.create_char("Conan", char_class=4, status=0x20)
        n = charlist_raw(vc)
        vc.close()

        print(f"oracle: {o.hex() if o else None}")
        print(f"v3    : {n.hex() if n else None}")
        ok = (o is not None and n is not None and o == n)
        print("OK: CHARLISTREPLY (with a real character + portrait) is "
              "byte-identical" if ok else "FAIL: charlist bodies differ")
        return 0 if ok else 1
    finally:
        v3.stop(); od.stop(); bnetd.stop()


sys.exit(main())
