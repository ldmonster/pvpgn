#!/usr/bin/env python3
"""Differential test: WOL unknown-verb (421 ERR_UNKNOWNCOMMAND) faithfulness.

The original server's handling of an unrecognized verb depends on login state:

  * BEFORE LOGIN: it emits a well-formed numeric
        :<server> 421 <nick> :Unrecognized command (before login)
    — a SINGLE trailing colon, NO echo of the offending command.
  * AFTER LOGIN: handle_irc_common_line routes the unknown verb to the bnet
    chat-command handler as "/<verb>", so NO 421 is sent at all (the observable
    is a server PAGE message carrying a localized "unknown command" text).

v3 previously emitted a malformed line in BOTH paths:
        :pvpgn.v3 421 * :FOOBAR :Unknown command      (before login)
        :pvpgn.v3 421 tester :FOOBAR :Unknown command (after login)
i.e. a DOUBLE colon (send_numeric injects " :" and the caller also embedded a
colon), it echoed the command, used the wrong text, and emitted a spurious 421
after login where the oracle sends none.

This guard verifies the corrected v3 behavior structurally against the oracle:
  - before login: exactly one well-formed 421 whose trailing param is the
    oracle's text and which does NOT echo the command;
  - after login: NO 421 is emitted.

Server name and the after-login PAGE payload are environment/localization
dependent, so we compare the decisive structure (numeric code, single-colon
trailing form, absence of a command echo, presence/absence of the 421), not
literal strings.

Run: python3 tests/diff/diff_wol_unknown.py
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from original_server import OriginalBnetd  # noqa: E402
from v3_server import V3Bnetd  # noqa: E402
import wol_client as wc  # noqa: E402

UNKNOWN_VERB = "FOOBAR"


def _drain(c, settle=0.5):
    time.sleep(settle)
    c.sock.settimeout(0.4)
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


def _find_421(lines):
    """Return the first 421 line, or None."""
    for ln in lines:
        if wc.WolClient.numeric(ln) == 421:
            return ln
    return None


def _trailing_param(line):
    """Return the IRC trailing parameter (text after the FIRST ' :'), or ''.

    A well-formed numeric has exactly one ' :' separator. A malformed double-colon
    line like '421 * :FOOBAR :Unknown command' has the verb before a SECOND colon.
    """
    idx = line.find(" :")
    return line[idx + 2:] if idx >= 0 else ""


def before_login(host, port):
    """Send only CVERS, then the unknown verb (no auth)."""
    c = wc.WolClient(host, port)
    c.send_line("CVERS 1 1000")
    _drain(c, 0.3)
    c.send_line(UNKNOWN_VERB)
    out = _drain(c, 0.5)
    c.close()
    line = _find_421(out)
    if line is None:
        return {"has_421": False}
    trailing = _trailing_param(line)
    return {
        "has_421": True,
        # Well-formed = single trailing colon: the command must NOT appear before
        # a second colon, i.e. there is no extra " :" buried in the message.
        "well_formed_single_colon": " :" not in trailing,
        "echoes_command": UNKNOWN_VERB in trailing,
    }


def after_login(host, port):
    """Log in fully, then send the unknown verb. Oracle sends NO 421."""
    c = wc.wol_session(host, port, "tester", "pw")
    if c is None:
        return None
    _drain(c, 0.3)
    c.send_line(UNKNOWN_VERB)
    out = _drain(c, 0.6)
    c.close()
    return {"has_421": _find_421(out) is not None}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--orig-repo", default="/home/cnupt/work/pvpgn-server")
    ap.add_argument(
        "--v3-bnetd",
        default=os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "../../build/v3-dev/src/app/bnetd/bnetd"))
    ap.add_argument("--orig-port", type=int, default=6362)
    ap.add_argument("--v3-port", type=int, default=6462)
    args = ap.parse_args()

    orig = OriginalBnetd(args.orig_repo, args.orig_port)
    v3 = V3Bnetd(args.v3_bnetd, args.v3_port)
    try:
        orig.start()
        v3.start()

        ob = before_login("127.0.0.1", orig.wolv1_port)
        nb = before_login("127.0.0.1", v3.wol_port)
        oa = after_login("127.0.0.1", orig.wolv1_port)
        na = after_login("127.0.0.1", v3.wol_port)
        if oa is None or na is None:
            print("FAIL: after-login WOL session setup failed")
            return 1

        print(f"{'field':<34}{'oracle':<10}{'v3':<10}match")
        print("-" * 62)
        all_ok = True

        # Before login: both must emit a well-formed, non-echoing 421.
        before_fields = ["has_421", "well_formed_single_colon", "echoes_command"]
        for f in before_fields:
            o, n = ob.get(f), nb.get(f)
            same = o == n
            all_ok &= same
            print(f"{'before.' + f:<34}{str(o):<10}{str(n):<10}"
                  f"{'OK' if same else 'DIFF'}")

        # After login: neither must emit a 421.
        same = oa["has_421"] == na["has_421"]
        all_ok &= same
        print(f"{'after.has_421':<34}{str(oa['has_421']):<10}"
              f"{str(na['has_421']):<10}{'OK' if same else 'DIFF'}")

        print()
        # The oracle must actually exhibit the faithful behavior we expect, and
        # v3 must match it.
        expected = (
            ob.get("has_421") is True
            and ob.get("well_formed_single_colon") is True
            and ob.get("echoes_command") is False
            and oa["has_421"] is False
        )
        if all_ok and expected:
            print("WOL unknown-verb matches the oracle (well-formed 421 before "
                  "login, no 421 after login).")
            return 0
        if not expected:
            print("FAIL: oracle did not exhibit the expected baseline behavior.")
        print("FAIL: WOL unknown-verb divergence.")
        return 1
    finally:
        v3.stop()
        orig.stop()


if __name__ == "__main__":
    sys.exit(main())
