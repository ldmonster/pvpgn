# 10 — Testing Pyramid Completion

## What

Close out the testing items still pending from wave-one plan 08, then
add sanitizer matrix, fuzz gate, and a coverage gate to CI.

## Pending from wave one

- Every `src/{domain,application}/<x>/src/*.cpp` has a paired
  `tests/unit/.../{*}_test.cpp`.
- No test in `tests/unit/` links `bnetd_legacy`,
  `integration_legacy_bnetd_*`, or `infra/{sqlite,mysql,postgres}/`.

## New in wave two

- ASan + UBSan + TSan CI matrix.
- Fuzz smoke as a required CI check.
- Coverage gate (fail PR on regression below threshold).
- Property-based tests for codecs and parsers.
- Mutation testing pilot on `domain/`.

## Why

- A green build with no sanitizers is a green build with hidden UB.
- Fuzz corpora exist (wave-one plan 08) but no scheduled run; bugs
  rot.
- Coverage today is reported but not enforced.

## Prerequisites

- Plans 02, 03, 04 in flight so unit tests stop being held back by
  legacy linkage.

## Concrete steps

1. **Pairing audit.** Script
   `scripts/dev/check-unit-pairing.sh` lists every
   `src/{domain,application}/**/*.cpp` without a matching test; CI
   fails on regression.
2. **Legacy-linkage ban.** Lint that fails any CMake `target_link_libraries`
   under `tests/unit/` referencing a legacy or infra-backend target.
3. **Sanitizer presets.** Add `v3-asan`, `v3-ubsan`, `v3-tsan` presets
   in `CMakePresets.json`. CI matrix runs each on Linux per PR.
4. **Fuzz gate.** Add a 5-minute fuzz smoke per target on PR; longer
   runs nightly. Use OSS-Fuzz-style harnesses under `tests/fuzz/`.
   Track regressions with reproducer corpora committed to
   `tests/fuzz/corpus/<target>/`.
5. **Coverage gate.** Use `llvm-cov` on `v3-coverage`; fail PR if
   `domain/` or `application/` coverage drops > 1% from main.
6. **Property tests.** Add rapidcheck (or in-tree generators) for:
   - bnet codec round-trip (encode→decode→encode is identity).
   - TOML schema validator (no panic on arbitrary input).
   - SRP session (mathematical invariants).
7. **Mutation testing pilot.** Run `mull` or equivalent over
   `domain/identity/` weekly; publish report. No gate yet.

## Acceptance criteria

- [ ] Pairing audit script in CI, currently passing.
- [ ] No `tests/unit/` target links any legacy or
      `infra/{sqlite,mysql,postgres}/` library.
- [ ] CI runs ASan + UBSan + TSan on every PR; all green on main.
- [ ] Fuzz smoke is a required check; reproducers stored on first
      finding.
- [ ] Coverage gate enforced; current floor documented in
      `docs/developer/testing.md`.

## Risks

- TSan + legacy code is a minefield. Run TSan only on v3 targets
  until plan 03 lands; gate the matrix appropriately.
- Fuzz smoke adds CI time. Cap per-PR fuzz to 5 minutes total wall
  across targets.

## Out of scope

- Replacing Catch2.
- E2E tests against real Battle.net clients (handled separately in
  `tests/e2e/`).
