# 14 — Migration Roadmap

Sequencing matters: each milestone is independently shippable, leaves the tree
green, and shrinks an allow-list or raises a test floor. Order is chosen so that
**safety nets come before risky moves** — we raise test coverage before we
delete legacy, so deletions are provably behaviour-preserving.

## Guiding rules

- **One coherent step at a time**, each verifiable locally, progress recorded in
  `docs/refactoring/progress.md`, commit only when asked.
- **Never weaken a gate to pass it**; mark env-gated items honestly.
- **Allow-lists only shrink.** A milestone that doesn't shrink one or raise a
  floor isn't done.

## Milestone 0 — Baseline & consolidation (low risk)

- Reconcile [02-current-state.md](02-current-state.md) against `git ls-files` and
  the layering/purity reports.
- Stand up `scripts/dev/check-all.sh` and the three pre-commit hooks
  ([11](11-local-quality-gates.md)).
- Collapse trackers into one ([13](13-documentation-and-adrs.md)).
- Add CTest labels.
- **Exit:** `check-all.sh` green (with honest skips); one tracker; one command
  to run gates.

## Milestone 1 — Raise the safety net (do this BEFORE more deletion)

- Backfill unit tests to green `check-unit-pairing.sh` across all layers.
- Build the protocol **golden + round-trip + fuzz** coverage
  ([07](07-protocol-layer.md)).
- Build the e2e **fake-client harness** + the journey catalogue
  ([10](10-e2e-and-functional.md)).
- **Exit:** `ctest -L unit,functional,e2e` green; protocol fuzz smoke clean;
  coverage ≥ 85% on domain+app.

## Milestone 2 — Finish the strangler (now safe)

- Delete the remaining `integration/legacy_*` bridges, `infra/legacy_config`,
  and `app/{d2cs,d2dbs}/legacy_*_bridges`, leaning on Milestone-1 e2e/golden
  tests to prove no behaviour change.
- Empty the `v3_layering_check.sh` allow-list.
- Prune dead one-shot `scripts/dev/*` (`plan0x_*`, `migrate_bridges_*`,
  `split_*`) via `check-scripts-orphans.sh`.
- **Exit:** no `legacy_*` dirs; layering allow-list empty; orphan-scripts check
  green.

## Milestone 3 — Domain & application hardening (DDD/SOLID/ISP)

- De-anemic the domain: move invariant logic into aggregates; introduce value
  objects for ids/names ([04](04-domain-layer.md)).
- Make cross-context links events-only.
- Audit ports for Interface Segregation; split read/write and
  capability-specific ports ([05](05-application-layer.md)).
- Replace residual globals/singletons with constructor injection
  ([08](08-cross-cutting.md)).
- **Exit:** purity green; no cross-context internal includes; no
  global/singleton access in domain/app; composition-root smoke tests pass.

## Milestone 4 — Infrastructure consolidation (env-gated where noted)

- One `IDbDriver`; delete the last per-backend repository code; run the
  **contract suite** over every available backend ([06](06-infrastructure-adapters.md),
  [09](09-testing-strategy.md)). *(env-gated: MySQL/Postgres)*
- Replace hand-rolled `fdwatch`/`hashtable`/`xstring` with one async runtime +
  stdlib ([12](12-build-and-dependencies.md)).
- Crypto: argon2id at-rest + `hash_version` upgrade-on-login; RNG port; no
  `std::rand`. *(env-gated: libsodium)*
- Observability ports with no-op default; OTLP export adapter. *(env-gated:
  libcurl + collector)*
- **Exit:** contract suite green on available backends; no bespoke primitives;
  crypto/observability designed + isolated-compile-verified where env-gated.

## Milestone 5 — Quality deepening

- Raise mutation kill ≥ 80% on `domain/identity`, then widen.
- Run sanitizer presets clean (asan/ubsan/tsan).
- Pin perf/footprint baselines with the local regression gate.
- **Exit:** `check-all.sh --deep` green (honest skips); baselines recorded.

## Milestone 6 — Docs, ADRs, release hygiene (local)

- Generated reference docs + `mkdocs build --strict` in `check-all.sh`.
- ADRs for all major decisions; one progress tracker finalized.
- Local release conveniences (distroless image, CHANGELOG discipline) without
  any CI ([13](13-documentation-and-adrs.md), [12](12-build-and-dependencies.md)).
- **Exit:** all Definition-of-Done checklists across sections ticked.

## Dependency order (what blocks what)

```
M0 ─▶ M1 ─▶ M2 ─▶ M3 ─▶ M4 ─▶ M5 ─▶ M6
        │            ▲
        └─ tests before deletion (M1 gates M2)
```

M1 must precede M2 (don't delete legacy without the safety net). M3/M4 can
interleave per context once M2 frees the tree. M5/M6 are continuous but
formalized at the end.

## Rollback

- Every milestone is a series of small commits behind the gates; any step that
  fails `check-all.sh` is reverted, not patched-over.
- Deletions in M2 are guarded by M1's golden/e2e tests — if a deletion changes a
  wire byte, the golden diff fails and the commit is reverted.
- Env-gated work that can't be locally verified is merged only as
  isolated-compiled + reference-reviewed, clearly marked in the tracker, and
  flipped to "verified" only once the backend/runtime is available.

## Final acceptance (the plan is "done")

All success criteria in [00-vision-and-scope.md](00-vision-and-scope.md) §4 hold
locally, and every section's Definition of Done is checked:

- [ ] Layering + domain-purity green with empty allow-lists.
- [ ] `check-unit-pairing.sh` green; coverage ≥ 85% domain+app; mutation ≥ 80%
      on identity.
- [ ] Every protocol family: golden + round-trip + fuzz.
- [ ] Every client journey: an asserted e2e scenario.
- [ ] No `legacy_*`; no bespoke `hashtable`/`xstring`/`fdwatch`; one `IDbDriver`.
- [ ] `check-all.sh` (+ `--deep`) green with honest skips; `mkdocs --strict`
      green; no CI files introduced.
