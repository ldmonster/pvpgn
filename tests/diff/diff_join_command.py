#!/usr/bin/env python3
"""The /join (and /channel //j) slash command joins a channel like SID_JOINCHANNEL.

The original maps /channel //join //j to _handle_channel_command, which calls
conn_set_channel -> the normal channel join, emitting EID_CHANNEL (0x07) with the
new channel name (+ roster). v3 had no handler, so /join fell through to the
"Unknown command" EID_ERROR. v3 now routes /join //channel //j through the
SID_JOINCHANNEL path.

Scenario: after login, send "/join <chan>" and look for an EID_CHANNEL (0x07)
naming that channel. The harness garbles localized text but the channel name is
plain ASCII, so compare the EID_CHANNEL presence + channel name on both servers.
"""
import argparse, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

EID_CHANNEL = 0x07
CHAN = "JoinCmdChan"


def probe(host, port):
    c, _ = bc.full_login(host, port, "jcmd", "pw")
    bc.drain_chat(c, 0.3)
    evs = bc.chat_command(c, f"/join {CHAN}")
    c.close()
    # find an EID_CHANNEL naming our target channel (case-insensitive)
    got = any(eid == EID_CHANNEL and CHAN.lower() in text.lower()
              for (eid, user, text) in evs)
    return {"channel_event": got,
            "eids": sorted({hex(eid) for (eid, _u, _t) in evs})}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6454)
    ap.add_argument("--v3-port", type=int, default=6554)
    ap.add_argument("--orig-only", action="store_true")
    a = ap.parse_args()
    orig = OriginalBnetd(a.orig_repo, a.orig_port)
    try:
        orig.start()
        o = probe("127.0.0.1", a.orig_port)
        print(f"oracle: {o}")
        if a.orig_only:
            return 0 if o["channel_event"] else 1
        v3 = V3Bnetd(a.v3_bnetd, a.v3_port)
        try:
            v3.start()
            n = probe("127.0.0.1", a.v3_port)
            print(f"v3    : {n}")
            ok = o["channel_event"] and n["channel_event"]
            print("OK: /join emits EID_CHANNEL for the named channel on both"
                  if ok else "FAIL")
            return 0 if ok else 1
        finally:
            v3.stop()
    finally:
        orig.stop()


sys.exit(main())
