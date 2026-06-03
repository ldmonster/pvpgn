# 09 — Testing Strategy

Testability is a *design* outcome (pure domain + injected ports), then a
*coverage* outcome. This section defines the pyramid and the doubles; the next
defines end-to-end specifically. All of it runs locally via CTest.

## 1. The pyramid

```
        ▲  fewer, slower, highest fidelity
        │   e2e        real server + scripted fake client (tests/e2e)
        │   integration adapter ↔ real backend (tests/integration)
        │   contract    one suite, every adapter (LSP)        (tests/unit + integration)
        │   functional  use-case + protocol round-trips        (tests/functional)
        │   unit         domain aggregates, use-cases w/ fakes  (tests/unit)
        ▼  many, fast, isolated
```

Existing dirs map directly: `tests/{unit,functional,integration,e2e,fuzz,bench,
abi}`. The mix should be **mostly unit**, a solid functional/contract band, and
a focused e2e top.

## 2. Unit tests (the base)

- Cover **every** domain aggregate/service and **every** application use-case.
- Link only `core` + the unit under test + `infra/inmemory` fakes — **never a
  real backend, socket, or clock.**
- Assert success *and* each typed error path.
- `tests/unit/<layer>/…` mirrors `src/<layer>/…`; `check-unit-pairing.sh`
  guarantees no source file lacks a paired test.

## 3. Test doubles (one home, no ad-hoc mocks)

- **Fakes over mocks.** In-memory implementations of ports live in
  `infra/inmemory` (repositories, event bus) and `tests/support` (fake clock,
  fake rng, fake responder). Reuse them; do not hand-roll a mock per test
  (DRY).
- A **fake clock / rng / id generator** makes time- and randomness-dependent
  use-cases deterministic.
- A **recording event publisher** lets tests assert which domain events fired.

## 4. Contract tests (LSP enforcement)

The same test body runs against every implementation of a port:

```
RepositoryContractTest<TDriver>  // parametrized
   ├─ InMemoryDriver   (always, fast)
   ├─ SqliteDriver     (when sqlite present)
   ├─ MySqlDriver      (when a MySQL is reachable)
   └─ PostgresDriver   (when a Postgres is reachable)
```

- Backends that aren't locally available are **skipped, loudly** (logged as
  skipped, not silently passed) so the run is honest on a constrained box.
- This is how we guarantee any backend is substitutable for the port.

## 5. Functional tests

- Drive a use-case end-to-end through its real application wiring with
  in-memory infra: command in → result + events + repository state asserted.
- Protocol functional tests: `bytes → handler → use-case (fakes) → bytes`,
  asserting the full adapter path without a socket.

## 6. Integration tests

- Adapter ↔ real dependency: `infra/sqlite` against a real temp DB file,
  `infra/net` against a real loopback socket, migrations applied for real.
- Gated by availability; `docker-compose.integration.yml` can stand up
  MySQL/Postgres locally for the developer who has Docker, but the suite must
  also degrade gracefully without it.

## 7. Property & fuzz

- Property tests for codecs (round-trip) and rating/pairing rules.
- Fuzz targets for every protocol framing/codec entrypoint
  ([07-protocol-layer.md](07-protocol-layer.md)), runnable as a local smoke
  (`fuzz-smoke`) under asan/ubsan.

## 8. Coverage & mutation (quality of tests, not just presence)

- **Coverage:** `v3-coverage` preset + `check-coverage.sh` enforce
  ≥ 85% line coverage on `domain/` + `application/` locally.
- **Mutation:** `mutation_pilot.py` mutates a module and asserts the suite kills
  the mutants; target ≥ 80% kill on `domain/identity`, then widen. Coverage says
  "lines ran"; mutation says "assertions matter."

## 9. Tasks for this plan

1. Backfill unit tests until `check-unit-pairing.sh` is green across all layers.
2. Build the parametrized repository **contract** harness and run it over every
   available backend.
3. Consolidate ad-hoc mocks into shared fakes (`infra/inmemory` +
   `tests/support`).
4. Raise `domain`+`application` coverage to ≥ 85% and widen the mutation pilot.
5. Wire all of the above as CTest labels (`unit`, `functional`, `integration`,
   `e2e`, `fuzz`, `contract`) so a dev can run any band by label.

## Definition of Done

- [ ] `ctest -L unit` is 100% green and `check-unit-pairing.sh` reports no
      unpaired source in any layer.
- [ ] A single contract suite runs against in-memory + every locally-available
      backend; unavailable backends are reported as skipped.
- [ ] No bespoke per-test mock duplicates a shared fake.
- [ ] `check-coverage.sh` ≥ 85% on `domain`+`application`; mutation kill ≥ 80%
      on `domain/identity`.
