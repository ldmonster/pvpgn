# 14 — Docs and `mkdocs build --strict`

## What

Finish the wave-one docs items that remain open, and make
`mkdocs build --strict` a required CI check.

## Pending from wave one

- `mkdocs build --strict` passes (currently fails on broken links and
  missing nav entries).
- Every page in `docs/` is reachable from `docs/index.md` in ≤ 3
  clicks.
- No file in `scripts/` is unreferenced by Dockerfile, CI, docs, or
  another script.

## New in wave two

- Generated config reference: schema → markdown.
- Generated plugin ABI reference: header → markdown (plan 12).
- Generated metrics reference: registry → markdown (plan 11).
- Per-bounded-context developer guide.
- Operator runbooks for the new observability stack.

## Concrete steps

1. **`mkdocs build --strict` gate.** Add to
   `.github/workflows/lint-layering.yml` (or split into a docs
   workflow). Fix every reported warning before flipping the gate.
2. **Nav audit.** Script `scripts/dev/check-docs-reachable.sh` walks
   `mkdocs.yml` nav and asserts every `docs/**/*.md` is reachable.
3. **Config reference generator.** `tools/gen-config-reference` reads
   the TOML schema (already wired in plan 11 of wave one) and emits
   `docs/reference/config-reference.md`. CI fails if generated output
   differs from committed.
4. **Plugin ABI reference.** `tools/gen-plugin-abi-reference` parses
   `include/pvpgn/plugin/abi.h` (plan 12) into
   `docs/reference/plugin-abi.md`.
5. **Metrics reference.** `tools/gen-metrics-reference` walks the
   registry to emit `docs/reference/metrics.md` (or a hand-maintained
   companion file checked for parity).
6. **Per-context developer guide.** One page per bounded context
   under `docs/developer/contexts/<ctx>.md` covering: aggregates,
   ports, adapters, where to add features. Auto-link from the
   reachability script.
7. **Operator runbooks** under `docs/operator/runbooks/`:
   - `connect-otel-collector.md`
   - `rolling-upgrade.md` (plan 15)
   - `rotate-argon2id-params.md` (plan 08)
   - `diagnose-slow-login.md`
   - `recover-from-corrupt-db.md`
8. **Scripts audit.** Walk `scripts/`, fail if a script is not
   referenced by Dockerfile, `.github/workflows/`, `docs/**`, or
   another script. Delete or document.

## Acceptance criteria

- [ ] `mkdocs build --strict` passes in CI as a required check.
- [ ] Every `.md` under `docs/` is reachable from `docs/index.md` in
      ≤ 3 clicks; CI gate.
- [ ] Generated reference pages are checked for drift in CI.
- [ ] One developer guide per bounded context exists.
- [ ] Operator runbooks listed above exist.
- [ ] `scripts/` has no orphan files; CI gate.

## Risks

- Generated docs drift. Treat the parity check failure as a normal
  PR fix, not a special case.
- Strict mode is noisy on first activation. Land in two PRs: one
  "fix all warnings", one "flip the gate".

## Out of scope

- Replacing mkdocs.
- Translating docs to additional languages beyond what already
  exists.
