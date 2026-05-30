# 13 — Execution roadmap

## Ordering principle

Each milestone has to leave `master` green, deployable, and behaviourally equivalent for operators who don't opt in to new features.

## Milestones

### M1 — Hygiene (low risk, high signal)

Plans: **02**, **12 (partial)**.

- Delete dead files, move `lua/`->`scripts/lua/`, normalize docs nav.
- No behaviour change for operators.
- Single PR per file group; reviewable in minutes.

Exit criteria: top-level ≤ 20 files, `mkdocs build --strict` green.

### M2 — Config consolidation

Plans: **03**, **04 (phase 4.1 relocate)**.

- Inline static `.conf` files into `bnetd.toml`.
- `pvpgn-migrate config` ships.
- v3 build no longer installs the legacy `.conf` files.

Exit criteria: `conf/` has 4 `.toml.in` files + `i18n/`.

### M3 — Layering enforcement

Plans: **07**, **08 (CI matrix)**, **09 (presets cleanup)**.

- Promote `v3_layering_check.sh` to required CI.
- Drop `legacy`/`linked`/`dev-release` presets.
- Coverage report wired up (no gate).

Exit criteria: `lint-layering` is a required check.

### M4 — Large-file decomposition (split-only, no logic changes)

Plans: **05**.

- One PR per big file (`handle_bnet_link.cpp`, `codec.cpp`, `bnet_protocol.h`, …).
- `.git-blame-ignore-revs` updated per PR.

Exit criteria: no non-vendored TU > 1500 LOC.

### M5 — Strangler completion

Plans: **06**, **04 (phase 4.2 modernize Lua)**.

- Drive every bridge to v3, promote, inline, delete legacy.
- Rename `bnetd-v3` → `bnetd`.
- Remove `PVPGN_BUILD_LEGACY` option.

Exit criteria: `src/integration/legacy_bnetd/` < 20 % of M4 LOC; `src/bnetd/` (legacy) gone.

### M6 — Observability + extensibility polish

Plans: **10**, **11**.

- JSON logging, mandatory per-context metrics, schema_version=3 enforcement.
- `docs/extending-pvpgn.md` published.

Exit criteria: external contributor can land a feature using only the docs.

### M7 — Modern C++ + xalloc removal

Plans: **05.2 (xalloc)**, **09 (warnings as errors)**.

- Final sweep: drop `src/common/{xalloc,xstr,asnprintf,scoped_*}`.
- `/WX` clean across the tree.

Exit criteria: `git grep xmalloc src/` returns nothing.

## Rollback strategy

Each milestone is reversible by reverting its merge commit. M5 is the only one that materially changes runtime behaviour; it ships behind a one-release `--use-legacy-codepaths` flag that disables the v3 promotion. Flag is removed in the release after.

## Versioning

- M1–M4 → patches under `3.0.x`.
- M5 → minor `3.1.0` (legacy code removed).
- M6 → minor `3.2.0`.
- M7 → minor `3.3.0` (compiler warning regime tightened).
- A breaking API change (Plugin ABI, Lua v2) requires `4.0.0`.

## Progress tracking

- `refactoring-progress.md` at repo root holds a flat list of checkboxes, one per acceptance criterion across all plan files.
- Per PR: tick the relevant boxes, link the PR, update `CHANGELOG.md` "Unreleased".

## When to stop a milestone early

A milestone is paused (not failed) if any of:

- An acceptance criterion can't be met without violating plan 01 (KISS/DRY/YAGNI).
- A planned removal turns out to have a live caller not in scope.
- Coverage drops > 5 percentage points relative to the milestone start.

In those cases: open an ADR explaining why, defer the criterion to a follow-up milestone, ship what's done.
