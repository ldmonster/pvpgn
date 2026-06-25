#!/usr/bin/env python3
"""Differential test: /who and /whois channel/user info commands.

Drives, against BOTH the original pvpgn-server and v3:

    alice + bob log in and join channel "TestChan"
    alice: /who TestChan   -> EID_INFO listing the channel's members
    alice: /whois bob      -> EID_INFO reporting bob's current channel

Observable compared: the SET of channel members /who reports (both servers must
list alice and bob). NOTE: the original localize()s its INFO replies and then
charset-converts them (i18n_convert); under this test harness (mock client with
no codepage negotiated) that mangles the LOCALIZED text — the "/who" prefix
"Users in channel X:" and the entire "/whois" line come back garbled. The
member names in /who are sprintf'd OUTSIDE localize, so they survive and are the
faithful, comparable observable. v3 emits clean text; its /whois (channel
location) is checked on the v3 side and guarded by a unit test, since the
oracle's /whois text is not byte-comparable in this harness.

Regression guard for the v3 /who and /whois handlers (channel_reader +
session_registry + account_repo).

Run: python3 tests/diff/diff_channelcmds.py
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import bncs_client as bc  # noqa: E402

CHAN = "TestChan"
EID_INFO = 0x12


EXPECTED = {"alice", "bob"}


def _info_texts(events):
    return [t for (eid, _u, t) in events if eid == EID_INFO]


def _who_members(events):
    """Members reported by /who — the expected usernames present as whitespace
    tokens in any INFO reply (robust to the oracle's garbled localized prefix)."""
    found = set()
    for t in _info_texts(events):
        for tok in t.split():
            low = tok.lower()
            if low in EXPECTED:
                found.add(low)
    return sorted(found)


def _whois_channel(events):
    for t in _info_texts(events):
        m = re.search(r'channel "([^"]+)"', t)
        if m:
            return m.group(1)
    return None


def scenario(host, port):
    out = {}
    alice, _ = bc.full_login(host, port, "alice", "alicepass")
    bob, _ = bc.full_login(host, port, "bob", "bobpass")
    bc.join_channel(alice, CHAN)
    bc.join_channel(bob, CHAN)

    out["who"] = _who_members(bc.chat_command(alice, f"/who {CHAN}"))
    out["whois_chan"] = _whois_channel(bc.chat_command(alice, "/whois bob"))

    alice.close()
    bob.close()
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6378)
    ap.add_argument("--v3-port", type=int, default=6478)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()
        o = scenario("127.0.0.1", args.orig_port)
        n = scenario("127.0.0.1", args.v3_port)
        print(f"{'field':<18}{'original':<20}{'v3':<20}note")
        print("-" * 66)
        # /who: decisive cross-server parity on the member set.
        who_match = (o["who"] == n["who"])
        print(f"{'who(members)':<18}{str(o['who']):<20}{str(n['who']):<20}"
              f"{'OK' if who_match else 'DIFF'}")
        # /whois: oracle text is charset-garbled in this harness (None); compared
        # v3-side only (must locate bob in TestChan).
        v3_whois_ok = (n["whois_chan"] == CHAN)
        print(f"{'whois(channel)':<18}{str(o['whois_chan']):<20}"
              f"{str(n['whois_chan']):<20}"
              f"{'v3 OK (oracle i18n-garbled)' if v3_whois_ok else 'v3 FAIL'}")
        success = (who_match and o["who"] == ["alice", "bob"] and v3_whois_ok)
        print()
        if success:
            print("/who matches the oracle (channel roster: alice, bob); "
                  "v3 /whois locates bob in the channel.")
            return 0
        print("FAIL: /who member set diverged, or v3 /whois wrong.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
