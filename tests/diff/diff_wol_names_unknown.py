#!/usr/bin/env python3
"""Differential test: WOL NAMES on an UNKNOWN channel falls back / stays silent.

The original's _handle_names_command (handle_wol.cpp) resolves the requested
channel; when it does not exist it falls back to the connection's CURRENT
channel (`if ((!channel) && (!(channel = conn_get_channel(conn)))) continue;`)
and replies with THAT channel's roster + name. If the user is in no channel at
all, it emits NOTHING — no 353, no 366.

v3 previously always built a roster from the (nonexistent) REQUESTED name and
always emitted an empty 353 + 366.

Decisive observables:
  Case A: in #RealCh, NAMES #NoSuch -> 353/366 carry the CURRENT channel name
          (#RealCh) and its real roster (operator '@'-prefixed).
  Case B: in NO channel, NAMES #NoSuch -> total silence (no 353, no 366).

Compares STRUCTURE (numeric presence, resolved channel name, operator marking),
not localized text or member order. Run: python3 tests/diff/diff_wol_names_unknown.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

UNKNOWN = "#NoSuchChannelXYZ"
REAL = "#RealCh"


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


def _scan(lines):
    """Return list of (code, channel_param, trailing) for 353/366 lines."""
    res = []
    for line in lines:
        parts = line.split()
        if len(parts) < 3:
            continue
        code = parts[1]
        if code not in ("353", "366"):
            continue
        colon = line.find(" :")
        trailing = line[colon + 2:] if colon >= 0 else ""
        # channel param is the last token before the trailing ':'
        head = line[:colon] if colon >= 0 else line
        chan = head.split()[-1]
        res.append((code, chan, trailing))
    return res


def scenario(host, port):
    # Case A: in a real channel, NAMES on a nonexistent channel.
    a = wc.wol_session(host, port, "alfa", "pw")
    if a is None:
        return None
    a.send_line(f"JOIN {REAL}")
    time.sleep(0.4)
    _drain(a)
    a.send_line(f"NAMES {UNKNOWN}")
    a_lines = _scan(_drain(a))
    a.close()

    # Case B: in NO channel, NAMES on a nonexistent channel.
    b = wc.wol_session(host, port, "bravo", "pw")
    if b is None:
        return None
    _drain(b)
    b.send_line(f"NAMES {UNKNOWN}")
    b_lines = _scan(_drain(b))
    b.close()

    a_353 = [x for x in a_lines if x[0] == "353"]
    a_366 = [x for x in a_lines if x[0] == "366"]
    return {
        # Case A: must fall back to the REAL (current) channel name, not UNKNOWN.
        "A_353_present": bool(a_353),
        "A_366_present": bool(a_366),
        "A_353_chan_is_real": bool(a_353) and a_353[0][1].lower() == REAL.lower(),
        "A_366_chan_is_real": bool(a_366) and a_366[0][1].lower() == REAL.lower(),
        "A_353_no_unknown": all(UNKNOWN.lower() not in x[1].lower() for x in a_lines),
        "A_op_alfa": bool(a_353) and "@alfa" in a_353[0][2],
        # Case B: total silence.
        "B_silent": len(b_lines) == 0,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6390)
    ap.add_argument("--v3-port", type=int, default=6490)
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
        fields = list(o.keys())
        print(f"{'field':<22}{'oracle':<10}{'v3':<10}match")
        print("-" * 50)
        all_ok = True
        for f in fields:
            same = o[f] == n[f]
            all_ok &= same
            print(f"{f:<22}{str(o[f]):<10}{str(n[f]):<10}{'OK' if same else 'DIFF'}")
        print()
        # Sanity: the oracle must actually exhibit the expected behaviour.
        oracle_ok = (o["A_353_chan_is_real"] and o["A_366_chan_is_real"]
                     and o["A_op_alfa"] and o["B_silent"])
        if all_ok and oracle_ok:
            print("WOL NAMES unknown-channel fallback matches the oracle.")
            return 0
        print("FAIL: WOL NAMES unknown-channel divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
