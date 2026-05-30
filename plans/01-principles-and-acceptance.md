# 01 — Principles and Definition of Done

## Hard rules (enforced by CI, not by convention)

### Layering (extends `scripts/v3_layering_check.sh`)

```
core         -> ( <std> only )
domain/*     -> core
application/*-> core, domain/*                ( NOT infra, NOT protocol, NOT integration )
protocol/*   -> core, domain/*                ( NOT application, NOT infra )
infra/*      -> core, domain/*                ( NOT application, NOT protocol )
integration/*-> everything below              ( the only legal merge point )
app/*        -> integration/*, application/*  ( wire-up only, no logic )
services/*   -> infra/*                       ( cross-cutting only )
runtime/*    -> core                          ( pure helpers )
```

`tests/unit/<layer>/...` may **only** include from `<layer>` and below.

### File-size budget (lint, not blocker)

| Kind                          | Soft cap | Hard cap |
|-------------------------------|----------|----------|
| domain / application `.cpp`   | 300 LOC  | 600 LOC  |
| infra adapter `.cpp`          | 500 LOC  | 1000 LOC |
| protocol codec `.cpp`         | 800 LOC  | 1500 LOC |
| integration bridge `.cpp`     | 400 LOC  | 800 LOC  |
| any header                    | 200 LOC  | 500 LOC  |
| vendored third-party          | n/a      | n/a      |

Anything currently over the hard cap is enumerated in [05-large-file-decomposition.md](05-large-file-decomposition.md).

### Public API surface

- Plugin C ABI (`pvpgn/plugin/api.h`) — append-only between minor versions, breaks bump major.
- Lua API v2 (`pvpgn.*`) — same rule.
- TOML schema — additive; renames require a deprecation window.
- C++ headers under `src/*/include/` — semver per library target.

## SOLID applied

- **S**ingle responsibility: a translation unit names one type or one feature, not "stuff".
- **O**pen/closed: extend by adding an adapter behind an existing port (`I*` interface in `domain/*/ports.hpp`), never by editing the domain service.
- **L**iskov: ports use value-returning, total functions. Pre-conditions are checked, not assumed.
- **I**nterface segregation: split fat ports the moment a consumer ignores half the methods.
- **D**ependency inversion: `application/<feature>` depends on `domain/<ctx>/ports.hpp`, never on `infra/<tech>/*`.

## KISS / DRY / YAGNI checklist (review gate)

For every PR, the author must be able to answer "yes" to all of:

- [ ] Does the change have a caller in this PR? (no speculative scaffolding)
- [ ] Are there ≤ 2 abstractions per added concept?
- [ ] Did I delete at least one obsolete code path when adding a new one?
- [ ] Did I avoid copying code I could have factored?
- [ ] Did I avoid adding a `TODO` for something I could fix in this PR?

## Definition of Done (per plan section)

A plan section ships when:

1. The code change is merged.
2. Layering check passes (`scripts/v3_layering_check.sh`).
3. `ctest --preset v3-release` is green.
4. `scripts/v3-e2e-*-smoke.sh` are green.
5. `refactoring-progress.md` checkbox flipped with PR link.
6. `CHANGELOG.md` "Unreleased" section updated.
