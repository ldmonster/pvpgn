#!/usr/bin/env python3
"""Differential test: SID_ICONREQ (0x2D) icon filename selection by clienttag.

The oracle (_client_iconreq) selects the icon filename by clienttag:
WAR3/W3XP clients get prefs_get_war3_iconfile() (default "icons-WAR3.bni");
all others get prefs_get_iconfile() (default "icons.bni").

We compare only the filename (deterministic), not the env-dependent timestamp.
"""
import sys

sys.path.insert(0, "/home/cnupt/work/pvpgn/tests/diff")
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as B

ORACLE_REPO = "/home/cnupt/work/pvpgn-server"
V3BIN = "/home/cnupt/work/pvpgn/build/v3-dev/src/app/bnetd/bnetd"
SID_ICONREQ = 0x2D


def icon_filename(host, port, product):
    c = B.BncsClient(host, port, timeout=3.0)
    try:
        try:
            B.auth_handshake(c, product=product)
        except Exception:
            # auth_check may fail for war3 products; clienttag is already
            # set at AUTH_INFO time, which is all SID_ICONREQ needs.
            pass
        c.send(SID_ICONREQ, b"")
        for _ in range(15):
            r = c.recv()
            if r is None:
                return "CLOSED"
            sid, b = r
            if sid == B.SID_PING:
                c.send(B.SID_PING, b[:4])
                continue
            if sid == SID_ICONREQ:
                # u64 timestamp + cstring filename
                return b[8:].split(b"\x00")[0].decode("latin1")
        return "NO_REPLY"
    finally:
        c.close()


EXPECTED = {
    b"SEXP": "icons.bni",
    b"STAR": "icons.bni",
    b"W3XP": "icons-WAR3.bni",
    b"WAR3": "icons-WAR3.bni",
}


def main():
    failures = []
    for product, expect in EXPECTED.items():
        o = OriginalBnetd(ORACLE_REPO, 11860)
        o.start()
        try:
            oo = icon_filename("127.0.0.1", 11860, product)
        finally:
            o.stop()
        v = V3Bnetd(V3BIN, 11866)
        v.start()
        try:
            vv = icon_filename("127.0.0.1", 11866, product)
        finally:
            v.stop()
        ok = oo == vv == expect
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] product={product.decode()}: "
              f"oracle={oo!r} v3={vv!r} expected={expect!r}")
        if not ok:
            failures.append(product.decode())
    if failures:
        print(f"\nFAILED for: {', '.join(failures)}")
        sys.exit(1)
    print("\nAll SID_ICONREQ filename cases match oracle.")


if __name__ == "__main__":
    main()
