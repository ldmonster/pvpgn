#!/usr/bin/env python3
"""Differential test: WOL LIST (RPL_LISTSTART 321 + RPL_CHANNEL 327 + RPL_LISTEND 323).

The original answers LIST with a 321, one 327 RPL_CHANNEL line per chat channel,
then a 323. Each 327 channel name is run through irc_convert_channel(): a leading
'#' is prepended and the name is escaped (space -> '_', etc.) so the channel token
is a single, valid IRC name on the wire.

v3 previously emitted the BARE store name in its 327 lines — no '#' prefix and
with embedded spaces left intact (e.g. "Diablo II"), which both diverges from the
oracle and splits the channel token on the wire. This guard joins a user channel
(#ListCh) and verifies the 327 entry for that user-created channel is well-formed:
'#'-prefixed name, member count 1, official flag 0 — matching the oracle.

The default/permanent channel SET is config-dependent and differs between servers
(and the WOLv1 ':' vs WOLv2 ' 388' line terminator + server name are environment-
dependent), so this compares the decisive observable: the user channel's 327 entry
plus the presence of the 321/323 envelope — not the literal default channel list.

Run: python3 tests/diff/diff_wol_list.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402


def _drain(c, settle=0.8):
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


def _parse_list(lines, chan):
    """Return observables for the LIST reply.

    chan is the '#'-prefixed channel token to look for (e.g. '#ListCh').
    """
    got_321 = got_323 = False
    liststart_params = None  # 321 params after the nick (server prefix + nick stripped)
    entry = None  # (count, official) for the wanted channel
    for line in lines:
        parts = line.split()
        if len(parts) < 2:
            continue
        code = parts[1]
        if code == "321":
            got_321 = True
            # ":<server> 321 <nick> Channel :Users Names" -> "Channel :Users Names".
            # Split off only the prefix/code/nick (first 3 tokens) but preserve
            # the trailing-param colon, so re-join from the raw line after them.
            idx = line.find(parts[2], line.find(code) + len(code))
            after_nick = line[idx + len(parts[2]):].strip()
            liststart_params = after_nick
        elif code == "323":
            got_323 = True
        elif code == "327" and len(parts) >= 6:
            # :server 327 nick <#name> <count> <official> <term>
            name = parts[3]
            if name == chan:
                # The official flag is a single digit. On WOLv1 the line ends
                # with ':' glued directly to it (e.g. "0:"); on WOLv2 with a
                # separate " 388". Normalize to just the leading digit so the
                # environment-dependent terminator does not skew the compare.
                official = parts[5].lstrip()[0] if parts[5] else ""
                entry = (parts[4], official)
    return {
        "got_321": got_321,
        "liststart_params": liststart_params,
        "got_323": got_323,
        "user_chan_present": entry is not None,
        "user_chan_count": entry[0] if entry else None,
        "user_chan_official": entry[1] if entry else None,
    }


def scenario(host, port):
    chan = "#ListCh"
    a = wc.wol_session(host, port, "alfa", "pw")
    if a is None:
        return None
    a.send_line(f"JOIN {chan}")
    time.sleep(0.4)
    _drain(a)
    a.send_line("LIST")
    res = _parse_list(_drain(a), chan)
    a.close()
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6396)
    ap.add_argument("--v3-port", type=int, default=6496)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", orig.wolv1_port)
        n = scenario("127.0.0.1", v3.wol_port)
        if not o or not n:
            print("FAIL: login/setup failed")
            return 1
        fields = ["got_321", "liststart_params", "got_323", "user_chan_present",
                  "user_chan_count", "user_chan_official"]
        print(f"{'field':<20}{'oracle':<12}{'v3':<12}match")
        print("-" * 52)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<20}{str(o[f]):<12}{str(n[f]):<12}{'OK' if same else 'DIFF'}")
        print()
        success = (all_ok and o["got_321"] and o["got_323"]
                   and o["user_chan_present"]
                   and o["liststart_params"] == "Channel :Users Names"
                   and n["liststart_params"] == "Channel :Users Names")
        if success:
            print("WOL LIST matches the oracle "
                  "(321 + '#'-prefixed 327 entry + 323).")
            return 0
        print("FAIL: WOL LIST divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
