# PvPGN Refactoring Plan

A comprehensive, **local-only** refactoring plan for PvPGN, organized so the
codebase obeys **SOLID**, **DDD**, **DRY**, **KISS**, and **YAGNI**, is more
**testable**, carries more **end-to-end** coverage, and is materially more
**maintainable and expandable**.

This plan is written from scratch. It does **not** assume any prior `plans/`
tree, and it deliberately contains **no GitHub/GitLab CI**: every quality gate
described here runs on a developer's machine via `pre-commit`, shell scripts in
`scripts/dev/`, and CMake/CTest targets. Wiring those same scripts into a CI
service later is trivial but explicitly out of scope.

## How to read this

Read the files in order. `00`–`03` establish the *why* and the *target*;
`04`–`08` define the *architecture* layer by layer; `09`–`11` define *how we
prove it works and keep it that way*; `12`–`14` cover *build, docs, and the
migration roadmap*. Every section ends with a **Definition of Done** checklist
phrased as locally verifiable facts.

| #  | File | Theme |
|----|------|-------|
| 00 | [00-vision-and-scope.md](00-vision-and-scope.md) | Why we refactor; non-goals; success criteria |
| 01 | [01-principles.md](01-principles.md) | SOLID/DDD/DRY/KISS/YAGNI made concrete for this repo |
| 02 | [02-current-state.md](02-current-state.md) | Honest inventory: what is done, what is debt |
| 03 | [03-target-architecture.md](03-target-architecture.md) | The layered/hexagonal target and the dependency rule |
| 04 | [04-domain-layer.md](04-domain-layer.md) | Bounded contexts, aggregates, ubiquitous language, events |
| 05 | [05-application-layer.md](05-application-layer.md) | Use-cases, ports, transactions, no-infra rule |
| 06 | [06-infrastructure-adapters.md](06-infrastructure-adapters.md) | Persistence, net, crypto, config, logging adapters |
| 07 | [07-protocol-layer.md](07-protocol-layer.md) | Codecs, framing, versioning, fuzzing surface |
| 08 | [08-cross-cutting.md](08-cross-cutting.md) | Errors, logging, config, time, DI/composition root |
| 09 | [09-testing-strategy.md](09-testing-strategy.md) | The test pyramid, doubles, contract tests, fixtures |
| 10 | [10-e2e-and-functional.md](10-e2e-and-functional.md) | E2E harness, fake clients, golden/scenario tests |
| 11 | [11-local-quality-gates.md](11-local-quality-gates.md) | pre-commit, scripts, sanitizers, coverage, mutation, bench |
| 12 | [12-build-and-dependencies.md](12-build-and-dependencies.md) | CMake presets, vendoring, optional-backend strategy |
| 13 | [13-documentation-and-adrs.md](13-documentation-and-adrs.md) | mkdocs, ADRs, generated reference docs |
| 14 | [14-migration-roadmap.md](14-migration-roadmap.md) | Sequencing, milestones, rollback, acceptance |

## Operating rules for executing this plan

1. **One coherent step at a time**, each independently verifiable; record
   progress in a tracker file and commit only when explicitly asked.
2. **Never weaken a gate to make it pass.** If a step is blocked by a missing
   local backend/runtime (SQLite, MySQL/Postgres, libsodium, Lua, Docker),
   mark it *env-gated* and verify by isolated `-Werror` compile + reference
   review rather than faking green.
3. **No new layering or purity violations.** The allow-lists in
   `scripts/v3_layering_check.sh` and `scripts/check_domain_purity.sh` may only
   shrink.
4. **YAGNI guards scope.** Do not introduce a port, abstraction, or backend
   until a second concrete consumer exists. Delete speculative code on sight.
