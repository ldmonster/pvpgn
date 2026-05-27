# 11 — Testing Strategy

**Goal:** A four-tier test pyramid in which the **unit** tier carries
most of the load, every layer has a clear contract, and CI never
ships red.

## 1. The pyramid

```
                    /\
                   /e2e\          ← tests/e2e/      (slow,  ~20)
                  /------\
                 /functional\     ← tests/functional/ (~50)
                /------------\
               /  integration  \  ← tests/integration/ (~150)
              /------------------\
             /        unit        \← tests/unit/        (~1000+)
            /----------------------\
           /         fuzz           \← tests/fuzz/     (continuous)
          --------------------------
```

(`tests/integration/` does not exist yet — it will be created in
R287.)

## 2. Tier definitions

### Unit (`tests/unit/`)
- Catch2 v3.5.4 (already in tree). Keep on this major.
- One TU per test file. No cross-test fixtures heavier than
  per-section.
- **Pure** — no sockets, no real DB, no real FS (use tmp directories
  via Catch2's `TempFile` helper if FS is unavoidable).
- Per layer:
  - `tests/unit/core/**` — utilities.
  - `tests/unit/domain/**` — value objects, entities, invariants.
  - `tests/unit/application/**` — use cases with **in-memory fakes**.
  - `tests/unit/protocol/**` — codecs (encode/decode/roundtrip).
  - `tests/unit/infra/**` — adapter unit tests (mocking the native
    handle where possible; or in-process SQLite/Lua).

### Functional (`tests/functional/`)
- Larger than unit; cross-component within a single binary.
- Real adapters where they are in-process (sqlite, lua). Mocked
  where they are out-of-process (mysql, postgres — covered by
  integration tier).

### Integration (`tests/integration/` — to be created)
- Spins up real out-of-process dependencies (MySQL, Postgres) via
  `docker compose` from `tests/integration/compose/`.
- Each test is a binary that connects to the compose stack, runs
  scenarios, asserts.
- Gated behind `PVPGN_V3_INTEGRATION_TESTS=ON` so dev laptops don't
  pay the cost.

### End-to-end (`tests/e2e/`)
- Black-box: launch real `bnetd`/`d2cs`/`d2dbs` binaries via
  `services::*` composition. A small Python harness in
  `scripts/dev/v3-compose-smoke.sh` already does the BNet
  handshake / chat / message round-trip. Extend to cover:
  - Account creation flow.
  - Channel lifecycle.
  - Game listing / join.
  - Realm registration (d2cs).
  - Character store (d2dbs).
- e2e runs nightly + on release branches. PRs run only the existing
  smoke.

### Fuzz (`tests/fuzz/`)
- Per protocol. See `06-protocol-and-codecs.md`.
- Also: TOML config parser, packet dispatcher, lua bridge inputs.

## 3. Property-based tests

Where applicable (codecs, value-object parsers, rating math), write
property tests using Catch2 generators. Keep examples small so they
double as documentation. Rapidcheck is optional — not added unless
the value is obvious.

## 4. Coverage

- gcov + lcov on Linux Debug CI run. Target: ≥ 80 % line, ≥ 70 %
  branch in `src/v3/core`, `src/v3/domain`, `src/v3/application`,
  `src/v3/protocol`.
- `src/v3/infra` is excluded from the gate — adapters need
  integration tier to cover, not unit.
- Coverage published as a CI artefact, not a hard gate (avoid
  Goodhart's law).

## 5. Sanitizers

CI matrix (see `12-build-tooling-ci.md`):

- Linux gcc Release + ctest.
- Linux clang Debug + ASan + UBSan + ctest.
- Linux clang Debug + TSan + ctest (catch race conditions in the
  event loop).
- Linux clang Debug + MSan (when libstdc++ is replaced or use
  libc++ + instrumented stdlib).
- Windows MSVC Release + ctest (existing).
- macOS clang Release + ctest.

The smoke compose runs **once per matrix axis**.

## 6. Test naming

- `tests/unit/<layer>/<area>/<thing>_test.cpp`.
- Test case names: present-tense imperative,
  `TEST_CASE("LoginAccount rejects unknown username", "[application][identity]")`.
- Tags: `[layer]`, `[bc]`, optional `[!slow]` / `[!benchmark]`.
- Keep ASCII (`/memories/windows-tooling.md` ctest unicode footgun).

## 7. Determinism

- All time-dependent tests use `core::FakeClock`.
- All randomness uses `core::SeededRandom` (deterministic seed in
  tests).
- Filesystem tests use temp dirs and clean up via Catch2 lifetimes.

## 8. Concrete tasks

- [ ] R287: create `tests/integration/` skeleton + docker-compose
      with MySQL 8 and Postgres 16.
- [ ] R288: backfill missing unit tests per `03-domain-purification.md`
      §4 catalogue.
- [ ] R289: wire coverage report into CI artefacts.
- [ ] R290: sanitizer matrix in CI.
- [ ] R291: fuzz nightly job.
