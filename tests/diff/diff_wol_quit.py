#!/usr/bin/env python3
"""Differential test: WOL QUIT reply wire format (607 RPL_QUIT :goodbye).

The original _handle_quit_command (handle_wol.cpp) answers a client QUIT with the
WOL-specific numeric RPL_QUIT (607) — irc_send formats it as
":<server> 607 <nick> :goodbye" — and then destroys the connection (the channel
PART to the other members comes from conn_quit_channel). v3's WolFsm::on_quit
previously sent a bare "ERROR :Closing Link", which no real WOL client expects.

    login WOL user -> QUIT -> read the reply line

and diffs the server-name-normalized reply on BOTH servers.

Run: python3 tests/diff/diff_wol_quit.py
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

USER = "quitter"


def _norm(line, server_name):
    """Replace the leading ':<server> ' source prefix with ':<S> '."""
    if line is None:
        return None
    if line.startswith(":"):
        parts = line.split(" ", 1)
        return ":<S> " + (parts[1] if len(parts) > 1 else "")
    return line


def scenario(host, wolport):
    c = wc.wol_session(host, wolport, USER, "pw")
    if c is None:
        return {"login": False, "reply": None}
    c.send_line("QUIT")
    reply = c.read_line()
    c.close()
    return {"login": True, "reply": reply}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=10920)
    ap.add_argument("--v3-port", type=int, default=10932)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    orig.start()
    v3.start()
    import time
    time.sleep(1)
    try:
        o = scenario("127.0.0.1", orig.wolv1_port)
        v = scenario("127.0.0.1", v3.wol_port)
    finally:
        orig.stop()
        v3.stop()

    on = _norm(o["reply"], None)
    vn = _norm(v["reply"], None)
    print("orig reply:", o["reply"])
    print("v3   reply:", v["reply"])
    print("orig norm :", on)
    print("v3   norm :", vn)

    ok = o["login"] and v["login"] and on == vn and on is not None \
        and " 607 " in on and ":goodbye" in on
    print("MATCH" if ok else "DIVERGENCE")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
