# SPDX-License-Identifier: GPL-2.0-or-later
"""Differential test: post-login chat/channel behaviour, original vs v3.

Logs into both servers, enters chat, joins a channel, and compares the
SID_CHATEVENT stream the server emits — the real client uses these to render the
channel and its user list.
"""
import argparse
import sys

sys.path.insert(0, __file__.rsplit("/", 1)[0])
from original_server import OriginalBnetd
from v3_server import V3Bnetd
import bncs_client as bc

EIDNAME = {1: "SHOWUSER", 2: "JOIN", 3: "LEAVE", 4: "WHISPER", 5: "TALK",
           6: "BROADCAST", 7: "CHANNEL", 9: "USERFLAGS", 0x12: "INFO",
           0x13: "ERROR", 0x17: "EMOTE"}


def scenario(host, port):
    out = {}
    try:
        c, uniq = bc.full_login(host, port, "chatuser", "secret")
        out["enter_chat_uniqname"] = uniq
        evs = bc.join_channel(c, "PvPGN")
        out["join_event_ids"] = [e[0] for e in evs]
        out["join_has_channel"] = any(e[0] == bc.EID_CHANNEL for e in evs)
        out["join_has_showuser"] = any(e[0] == bc.EID_SHOWUSER for e in evs)
        out["join_has_userflags"] = any(e[0] == 0x09 for e in evs)
        # the user must appear in the channel roster (SHOWUSER for self)
        out["self_in_roster"] = any(
            e[0] == bc.EID_SHOWUSER and e[1] == "chatuser" for e in evs)
        c.close()
    except Exception as e:
        out["error"] = f"{type(e).__name__}: {e}"
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument("--v3-bnetd", required=True)
    ap.add_argument("--orig-port", type=int, default=6350)
    ap.add_argument("--v3-port", type=int, default=6450)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    orig.start()
    try:
        v3.start()
        try:
            o = scenario("127.0.0.1", args.orig_port)
            v = scenario("127.0.0.1", args.v3_port)
        finally:
            v3.stop()
    finally:
        orig.stop()

    def fmt(seq):
        if isinstance(seq, list):
            return "[" + ",".join(EIDNAME.get(x, hex(x)) for x in seq) + "]"
        return str(seq)

    print(f"{'field':<22} {'original':<34} {'v3'}")
    print("-" * 80)
    for k in sorted(set(o) | set(v)):
        print(f"{k:<22} {fmt(o.get(k)):<34} {fmt(v.get(k))}")

    # The security/usability-critical assertion: the joining user must see the
    # channel name AND appear in the roster, like the oracle.
    print()
    gaps = []
    for k in ("join_has_channel", "join_has_showuser", "self_in_roster"):
        if o.get(k) and not v.get(k):
            gaps.append(k)
    if gaps:
        print(f"v3 CHANNEL-JOIN GAPS vs oracle: {', '.join(gaps)}")
        return 1
    print("channel-join event stream matches the oracle on the key events.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
