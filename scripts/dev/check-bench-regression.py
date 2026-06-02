#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# scripts/dev/check-bench-regression.py -- Plan 13 microbench regression gate.
#
# Compares a fresh bench-results.json against a committed baseline and fails if
# any benchmark's median ns/op exceeds the baseline by more than the budget
# (default 10%). Per the plan's Risks note the comparison is on the MEDIAN, not
# the worst sample. Absolute ns/op are host-specific, so the baseline must have
# been captured on the SAME runner as `current` (e.g. CI captures main's
# baseline on the CI runner) -- see docs/developer/benchmarking.md.
#
# Usage: check-bench-regression.py <baseline.json> <current.json> [--budget PCT]
# Exit:  0 = within budget (or only new benchmarks), 1 = a regression.

import argparse
import json
import sys


def load(path):
    with open(path) as f:
        data = json.load(f)
    return {b["name"]: b for b in data.get("benchmarks", [])}


def main() -> int:
    ap = argparse.ArgumentParser(description="Microbench regression gate (Plan 13).")
    ap.add_argument("baseline")
    ap.add_argument("current")
    ap.add_argument("--budget", type=float, default=10.0,
                    help="allowed regression in percent (default 10)")
    args = ap.parse_args()

    base = load(args.baseline)
    cur = load(args.current)

    fail = 0
    print(f"microbench regression gate (budget {args.budget:.0f}% over baseline)")
    for name in sorted(cur):
        c = cur[name]["ns_per_op"]
        if name not in base:
            print(f"  NEW      {name}: {c:.2f} ns (no baseline entry)")
            continue
        b = base[name]["ns_per_op"]
        limit = b * (1.0 + args.budget / 100.0)
        delta = 100.0 * (c - b) / b if b else 0.0
        ok = c <= limit
        if not ok:
            fail = 1
        print(f"  [{'OK' if ok else 'REGRESS'}] {name}: {c:.2f} ns vs base "
              f"{b:.2f} ns ({delta:+.1f}%)")

    for name in sorted(base):
        if name not in cur:
            print(f"  MISSING  {name}: in baseline, absent from current run")

    if fail:
        print("\nFAIL: a benchmark regressed beyond the budget. If this is an "
              "intentional, justified change, recapture the baseline in the same "
              "PR and explain why in the description.")
    else:
        print("\nOK: all benchmarks within budget.")
    return fail


if __name__ == "__main__":
    sys.exit(main())
