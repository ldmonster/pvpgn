# 08 — Testing strategy

## Pyramid

```
                e2e (real bnetd-v3 + real client mocks)
              ────────────────────────────────────────
                integration (process boundary mocked)
              ────────────────────────────────────────
                functional (single app + fake infra)
              ────────────────────────────────────────
                unit  (per-class / per-function)
```

Existing layout already matches: `tests/{unit,functional,integration,e2e,fuzz}`. Goal: **enforce ratios**, not add a new framework.

## Per-layer rules

### Unit (`tests/unit/`)

- One test file per source `.cpp`/`.hpp` unit. Mirror the directory tree.
- May depend on: `core`, the unit under test, its direct dependency. **Nothing else** — no SQLite, no network, no Lua.
- Catch2 v3.5.4 (see user memory note re `StringMaker<std::byte>` workaround).
- Soft target: ≥ 80 % line coverage in `domain/` and `application/`. Tracked by `v3-coverage` preset.

### Functional (`tests/functional/`)

- Boot one application layer use-case with `InMemoryUnitOfWork`. Exercise via its command/query type.
- May depend on: `application/<feature>`, `infra/inmemory/`. No real persistence, no real net.

### Integration (`tests/integration/`)

- Boot a sub-system (e.g. chat) with **real** infra adapters (SQLite, Asio sockets on loopback).
- Allowed to spawn a process. Allowed to use `docker-compose.v3.yml` profile `integration`.

### E2E (`tests/e2e/`)

- The four `scripts/v3-e2e-*-smoke.sh` scripts. Drive a real `bnetd-v3` via real `bnbot`/`bnchat`/`bnftp`/`bnstat`. Reference the user-memory note: `nc -z` is forbidden as a readiness probe.

### Fuzz (`tests/fuzz/`)

- libFuzzer targets for each protocol decoder (`bnet`, `irc`, `wol`, `telnet`, `d2cs`, `d2gs`). Existing `v3-fuzz` preset is the harness. CI runs ≥ 5 minutes per target on PRs labelled `fuzz`.

## CI matrix

| Job             | Preset           | When |
|-----------------|------------------|------|
| unit + lint     | `v3-dev`         | every PR |
| asan + ubsan    | `v3-asan`        | every PR |
| tsan            | `v3-tsan`        | every PR (chat + matchmaking only — TSan is slow) |
| coverage report | `v3-coverage`    | every PR (no threshold gate until plan 06 done) |
| layering check  | `v3-dev`         | every PR (required, see plan 07) |
| clang-tidy      | `v3-dev`         | every PR (warnings as errors on changed files) |
| fuzz quick      | `v3-fuzz`        | every PR, 60 s budget per target |
| fuzz long       | `v3-fuzz`        | nightly, 30 min budget per target |
| docs            | mkdocs           | every PR touching `docs/` |

## Catch2 conventions (lock in user-memory gotchas)

- Test names ASCII-only (`ctest` filter is not Unicode-safe).
- For `vector<std::byte>` equality, byte-loop or `static_cast<uint8_t>`; never direct `==`.
- In `tests/unit/infra/legacy_config/*`: cast `StatusCode` via `static_cast<int>`.
- In `tests/unit/application/anongame_inforeply/*`: direct `core::StatusCode` comparisons OK.

## Acceptance criteria

- [ ] Every `src/{domain,application}/<x>/src/*.cpp` has a paired `tests/unit/.../{*}_test.cpp`.
- [ ] Coverage report ≥ 80 % for `domain` and `application` (excluding strangler bridges).
- [ ] No test in `tests/unit/` links `bnetd_legacy`, `integration_legacy_bnetd_*`, or `infra/{sqlite,mysql,postgres}/`.
- [ ] Fuzz corpora are checked in under `tests/fuzz/corpus/<target>/`.

## Risks

- Coverage as a hard gate too early stalls strangler work. Keep it as a report-only metric until plan 06 closes.

## Out of scope

- Switching test framework. Catch2 stays.
