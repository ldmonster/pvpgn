# 16 — Execution Roadmap

## Ordering rationale

Wave two is bottom-up. Each row lists prerequisites; rows in the same
phase can run in parallel.

## Phase A — Foundations (parallel where possible)

| Plan | Why first | Blocks |
|------|-----------|--------|
| 05 ports-consolidation | Mechanical, unblocks reviewers' mental model | — |
| 03 strangler-finalization | Unlocks 02, 06, 09 | 02, 06, 09 |
| 04 d2cs-d2dbs-strangler | Sibling to 03, can run after 03 patterns settle | 02 (d2 parts) |
| 14 docs-and-mkdocs-strict | Cheap, prevents doc-debt accumulation | — |

## Phase B — Clean floor (after 03)

| Plan | Why now |
|------|---------|
| 02 common-purge | Now safe — legacy callers gone |
| 07 infra-adapter-rehab | Schema and repository cleanup before async |
| 08 crypto-modernization | Touches `common/`; do alongside 02 |
| 10 testing-pyramid-completion | Tighten gates before risky migrations |

## Phase C — Modern runtime (after B)

| Plan | Why now |
|------|---------|
| 06 async-io-modernization | Floor is clean; async migration is safer |
| 09 cpp23-uplift | Toolchain bump after legacy-tree influence is gone |

## Phase D — External surface (after C)

| Plan | Why now |
|------|---------|
| 11 observability-otel | Tracing meaningful only on real executor |
| 12 plugin-abi-stabilization | Stable hooks ride on stable runtime |
| 13 perf-benchmark-baseline | Bench what the new runtime actually does |

## Phase E — Ship (after D)

| Plan | Why last |
|------|----------|
| 15 release-and-rollout | Codify the upgrade story once the changes exist |

## Milestone calendar (relative)

- **M1 (Phase A complete):** legacy integration trees gone; ports
  consolidated; docs strict-clean.
- **M2 (Phase B complete):** `src/common/` empty; one repository per
  aggregate; argon2id default for new accounts; coverage + sanitizer
  gates enforced.
- **M3 (Phase C complete):** asio runtime live; C++23 enforced; no
  platform `#ifdef` outside `infra/`.
- **M4 (Phase D complete):** OTLP exporter live; plugin ABI v1
  frozen; benchmark gate enforced.
- **M5 (Phase E complete):** wave-two release tagged, signed,
  rolled out to a reference deployment with no downtime.

## Rollback strategy

- Every plan's rollback is "revert the merge commit" unless noted.
- Plans with persistent-state implications (07 schema, 08 password
  hash) ship a forward-compatible window so a rollback within that
  window is safe.
- Plan 06 (async I/O) lands one listener at a time; rollback granularity
  is one listener.
- Plan 09 (C++23) is gated by toolchain matrix; a single compiler
  failure on `main` triggers an immediate revert and floor adjustment.

## Tracking

Add a new section to `refactoring-progress.md`:

```markdown
## Wave Two

### Phase A
- [ ] Plan 05 — application/ports/ collapsed
- [ ] Plan 03 — src/integration/legacy_bnetd/ deleted
- [ ] Plan 04 — d2cs / d2dbs stranglered
- [ ] Plan 14 — mkdocs --strict green in CI

### Phase B
- [ ] Plan 02 — src/common/ purged
- [ ] Plan 07 — infra adapters rehabbed
- [ ] Plan 08 — argon2id default
- [ ] Plan 10 — sanitizer + fuzz + coverage gates live

### Phase C
- [ ] Plan 06 — asio runtime live
- [ ] Plan 09 — C++23 enforced

### Phase D
- [ ] Plan 11 — OTLP exporter live
- [ ] Plan 12 — plugin ABI v1 frozen
- [ ] Plan 13 — bench gate enforced

### Phase E
- [ ] Plan 15 — wave-two release tagged
```
