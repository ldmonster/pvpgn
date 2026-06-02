#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# scripts/dev/mutation_pilot.py -- Plan 10, step 7: mutation-testing pilot.
#
# A small, dependency-free mutation tester ("mull or equivalent" per the plan).
# For each (source, test-target) pair it generates one mutant at a time by
# swapping a single operator token, rebuilds the paired test target, and runs
# it:
#
#   * test FAILS  -> the mutant was KILLED (the suite noticed the change). Good.
#   * test PASSES -> the mutant SURVIVED  (a behaviour change no test catches).
#                    Each survivor is a concrete "write a test here" pointer.
#   * won't build -> counted as killed-by-compile (the change is not viable).
#
# It mutates only operator tokens in *code* (comments and string/char literals
# are skipped) using a low-false-positive operator set. This is a pilot, not a
# gate: it reports a mutation score and the surviving mutants; it never fails
# the build. Run weekly (see .github/workflows/mutation.yml).
#
# Usage:
#   python3 scripts/dev/mutation_pilot.py --build build [--max-mutants N]
#                                         [--json report.json]
# Defaults target domain/identity/ (Plan 10's pilot scope).

import argparse
import json
import subprocess
import sys
from dataclasses import dataclass, field, asdict
from pathlib import Path

# (source file, paired ctest test target) pairs to mutate. Plan 10 scopes the
# pilot to domain/identity/. The aggregate's branching logic lives in the
# headers (account.hpp / attribute_map.hpp); mutating a header only recompiles
# the single target we rebuild, so the pilot stays fast.
DEFAULT_TARGETS = [
    ("src/domain/identity/include/domain/identity/account.hpp",
     "test_domain_identity_account"),
    ("src/domain/identity/include/domain/identity/attribute_map.hpp",
     "test_domain_identity_attribute_map"),
]

# Single-token operator swaps. Deliberately conservative: comparison/boolean
# operators are semantically meaningful and rarely appear in a way that breaks
# compilation when swapped, which keeps the signal (survivors) meaningful.
# Order matters: longer tokens first so "<=" is matched before "<".
MUTATIONS = [
    ("==", "!="),
    ("!=", "=="),
    ("<=", "<"),
    (">=", ">"),
    ("&&", "||"),
    ("||", "&&"),
]


def code_spans(text: str):
    """Yield (start, end) byte offsets of regions that are real code, i.e. not
    inside // line comments, /* block comments */, "strings", or 'chars'."""
    i, n = 0, len(text)
    span_start = 0
    out = []
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            out.append((span_start, i))
            j = text.find("\n", i)
            i = n if j < 0 else j
            span_start = i
        elif two == "/*":
            out.append((span_start, i))
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            span_start = i
        elif c in ('"', "'"):
            out.append((span_start, i))
            quote = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    break
                j += 1
            i = j + 1
            span_start = i
        else:
            i += 1
    out.append((span_start, n))
    return out


def in_code(offset: int, spans) -> bool:
    return any(s <= offset < e for s, e in spans)


@dataclass
class Mutant:
    file: str
    line: int
    col: int
    frm: str
    to: str
    status: str = "pending"  # killed | survived | killed_build


@dataclass
class Report:
    total: int = 0
    killed: int = 0
    survived: int = 0
    killed_build: int = 0
    capped: int = 0  # sites discovered but not run due to --max-mutants
    mutants: list = field(default_factory=list)

    @property
    def score(self) -> float:
        run = self.killed + self.survived + self.killed_build
        return 0.0 if run == 0 else 100.0 * (self.killed + self.killed_build) / run


def find_sites(text: str, rel: str):
    spans = code_spans(text)
    sites = []
    for frm, to in MUTATIONS:
        start = 0
        while True:
            idx = text.find(frm, start)
            if idx < 0:
                break
            start = idx + 1
            if not in_code(idx, spans):
                continue
            # Avoid mutating a token that is really part of a longer operator
            # (e.g. don't turn "<<=" or "==" inside "===" -- C++ has no === but
            # guard "<=" inside "<<=" and "==" adjacency anyway).
            prev = text[idx - 1] if idx > 0 else ""
            nxt = text[idx + len(frm)] if idx + len(frm) < len(text) else ""
            if frm in ("==", "!=") and (prev in "=!<>" or nxt == "="):
                continue
            if frm in ("<=", ">=") and (prev in "<>" or nxt == "="):
                continue
            if frm in ("&&", "||") and (prev in "&|" or nxt in "&|"):
                continue
            line = text.count("\n", 0, idx) + 1
            col = idx - (text.rfind("\n", 0, idx))
            sites.append((idx, frm, to, line, col))
    sites.sort()
    return sites


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL, **kw)


def main() -> int:
    ap = argparse.ArgumentParser(description="Mutation-testing pilot (Plan 10).")
    ap.add_argument("--build", default="build", help="configured build dir")
    ap.add_argument("--max-mutants", type=int, default=20,
                    help="cap mutants actually run (0 = no cap)")
    ap.add_argument("--json", help="write the machine-readable report here")
    ap.add_argument("--repo", default=".", help="repo root")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    build = (repo / args.build).resolve()
    if not (build / "CMakeCache.txt").exists():
        print(f"error: {build} is not a configured CMake build dir", file=sys.stderr)
        return 2

    report = Report()
    print(f"mutation pilot: build={build} max-mutants={args.max_mutants or 'all'}\n")

    for rel, target in DEFAULT_TARGETS:
        src = repo / rel
        if not src.exists():
            print(f"  skip {rel}: not found")
            continue
        original = src.read_text()
        sites = find_sites(original, rel)
        print(f"  {rel} -> {target}: {len(sites)} mutable site(s)")

        # Baseline: the target must build and its tests must pass unmutated, or
        # results are meaningless.
        if run(["cmake", "--build", str(build), "--target", target]).returncode != 0:
            print(f"    ! baseline build of {target} failed; skipping")
            continue
        binp = next(build.rglob(target), None)
        if binp is None or run([str(binp)]).returncode != 0:
            print(f"    ! baseline test {target} not green; skipping")
            continue

        for (idx, frm, to, line, col) in sites:
            if args.max_mutants and (report.killed + report.survived +
                                     report.killed_build) >= args.max_mutants:
                report.capped += 1
                continue
            m = Mutant(file=rel, line=line, col=col, frm=frm, to=to)
            mutated = original[:idx] + to + original[idx + len(frm):]
            try:
                src.write_text(mutated)
                built = run(["cmake", "--build", str(build), "--target", target])
                if built.returncode != 0:
                    m.status = "killed_build"
                    report.killed_build += 1
                else:
                    tested = run([str(binp)])
                    if tested.returncode != 0:
                        m.status = "killed"
                        report.killed += 1
                    else:
                        m.status = "survived"
                        report.survived += 1
            finally:
                src.write_text(original)
            report.total += 1
            report.mutants.append(asdict(m))
            mark = {"killed": "x", "survived": "SURVIVED", "killed_build": "b"}[m.status]
            print(f"    {rel}:{line}:{col}  {frm} -> {to}   [{mark}]")

        # Make sure the target is rebuilt clean from the restored source.
        run(["cmake", "--build", str(build), "--target", target])

    print(f"\nmutation score: {report.score:.1f}%  "
          f"(killed {report.killed + report.killed_build}, "
          f"survived {report.survived}, of {report.total} run"
          f"{f'; {report.capped} more capped' if report.capped else ''})")
    if report.survived:
        print("\nSURVIVING MUTANTS (each is a gap a test could close):")
        for m in report.mutants:
            if m["status"] == "survived":
                print(f"  {m['file']}:{m['line']}:{m['col']}  "
                      f"{m['frm']} -> {m['to']}")

    if args.json:
        payload = asdict(report)
        payload["score"] = round(report.score, 2)
        Path(args.json).write_text(json.dumps(payload, indent=2))
        print(f"\nwrote {args.json}")

    # Pilot, not a gate: always succeed (CI uploads the report as an artifact).
    return 0


if __name__ == "__main__":
    sys.exit(main())
