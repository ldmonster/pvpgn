# 11 — Local Quality Gates (No CI)

Every invariant in this plan is enforced **on the developer's machine**, not by
a hosted pipeline. The toolbox already exists under `scripts/dev/` and
`CMakePresets.json`; this section makes it a single, coherent, runnable gate.

> **Explicit non-goal:** no `.github/workflows`, no `.gitlab-ci.yml`. The same
> scripts *could* be dropped into a CI runner later, but authoring CI is out of
> scope.

## 1. The three rings of enforcement

1. **`pre-commit` (fast, every commit).** Formatting and cheap structural
   checks — seconds.
2. **`scripts/dev/check-all.sh` (medium, before push / on demand).** All
   architectural + test gates that finish in a couple of minutes on a dev box.
3. **On-demand deep gates (slow, when relevant).** Sanitizers, coverage,
   mutation, fuzz, bench — run when touching the relevant area or before a
   release tag.

## 2. Ring 1 — `pre-commit`

Already configured (`.pre-commit-config.yaml`): trailing-whitespace,
end-of-file, yaml/json checks, line endings, large-file guard, `clang-format`,
`cmake-format`/`cmake-lint`. **Add** fast structural hooks:

- `v3_layering_check.sh` (allow-list may only shrink),
- `check_domain_purity.sh src/domain`,
- `check-changelog.sh` (Keep-a-Changelog discipline).

## 3. Ring 2 — `scripts/dev/check-all.sh` (new aggregator)

A single script that runs, fails fast, and prints a green/red summary:

| Gate | Script | Enforces |
|------|--------|----------|
| Layering | `v3_layering_check.sh` | dependency rule, empty allow-list |
| Domain purity | `check_domain_purity.sh` | no I/O/clock/global in domain |
| Unit pairing | `check-unit-pairing.sh` | every source has a paired test |
| Build + unit | CMake `v3-dev` + `ctest -L unit` | 100% green |
| Functional | `ctest -L functional` | use-case/protocol paths |
| Config sync | `check-config-reference-sync.sh` | docs match schema |
| Docs reachable | `check-docs-reachable.sh` | no orphan docs |
| Plugin ABI | `check-plugin-abi.sh` + `-purity.sh` | semver + no symbol leak |
| Script orphans | `check-scripts-orphans.sh` | no dead one-shot scripts |
| Test↔legacy | `check-test-legacy-linkage.sh` | tests don't pull legacy |

## 4. Ring 3 — deep gates (CMake presets + scripts)

| Gate | How | Target |
|------|-----|--------|
| AddressSanitizer | preset `v3-asan` + `ctest` | no leaks/overflows |
| UBSan | preset `v3-ubsan` | no UB |
| ThreadSanitizer | preset `v3-tsan` | no races |
| Coverage | preset `v3-coverage` + `check-coverage.sh` | ≥ 85% domain+app |
| Mutation | `mutation_pilot.py` | ≥ 80% kill (widening) |
| Fuzz smoke | `fuzz-smoke` target | no crash/UB on corpus |
| Bench regression | `run-bench.sh` + `check-bench-regression.py` | within baseline |
| E2E | `ctest -L e2e` | journeys pass ([10](10-e2e-and-functional.md)) |

## 5. Make it one command

Add CMake/CTest convenience so a developer never needs to remember script
paths:

```
cmake --preset v3-dev && cmake --build --preset v3-dev
ctest --preset v3-dev            # unit + functional
scripts/dev/check-all.sh         # all ring-2 gates
scripts/dev/check-all.sh --deep  # + sanitizers/coverage/mutation/fuzz/bench
```

`check-all.sh` is the local stand-in for "CI is green."

## 6. Honesty rule for constrained boxes

When a backend/runtime is missing (no sqlite/libsodium/Lua/Docker), the gate
**reports the item as skipped with the reason**, and the env-gated work is
verified by isolated `-Werror` compile + reference review. A skip is never
silently counted as a pass. (This mirrors the project's existing
"reference-verified vs build-verified" discipline.)

## 7. Tasks for this plan

1. Write `scripts/dev/check-all.sh` aggregator (+ `--deep`).
2. Add the three structural hooks to `.pre-commit-config.yaml`.
3. Add CTest labels (`unit/functional/integration/e2e/fuzz/contract`) and a
   `fuzz-smoke` target.
4. Prune dead one-shot scripts via `check-scripts-orphans.sh` until it's green.
5. Document the three rings in `CONTRIBUTING.md`.

## Definition of Done

- [ ] `scripts/dev/check-all.sh` exists, runs all ring-2 gates, and is green
      (skips reported honestly) on a clean checkout.
- [ ] `pre-commit run --all-files` includes layering + purity + changelog hooks
      and passes.
- [ ] `--deep` runs sanitizers/coverage/mutation/fuzz/bench locally.
- [ ] No CI pipeline files are introduced by this plan.
