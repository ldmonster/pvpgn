# Testing Strategy

PvPGN uses a four-layer test pyramid. Each layer has strict rules about what it
may depend on.

## Test Pyramid

```
         ┌─────────────────┐
         │   e2e (smoke)   │  Real bnetd + real client tools
         ├─────────────────┤
         │   integration   │  Real infra adapters (SQLite, Asio loopback)
         ├─────────────────┤
         │   functional    │  Single use-case + InMemory adapters
         ├─────────────────┤
         │      unit       │  Per-class / per-function; no I/O
         └─────────────────┘
```

## Layer Rules

### Unit (`tests/unit/`)

- One test file per source `.cpp`/`.hpp` unit; mirror the directory tree
- May depend on: `core`, the unit under test, its direct dependencies
- **Forbidden**: SQLite, network sockets, Lua VM, `bnetd_legacy`, `integration_legacy_bnetd_*`
- Framework: Catch2 v3.5.4
- Target: ≥ 80% line coverage in `domain/` and `application/`

### Functional (`tests/functional/`)

- Boot one application layer use-case with `InMemoryUnitOfWork`
- No real persistence, no real network

### Integration (`tests/integration/`)

- Boot a sub-system with real infra adapters (SQLite, Asio sockets on loopback)
- May spawn a process; may use `docker-compose.v3.yml` profile `integration`
- Opt-in: `PVPGN_V3_INTEGRATION_TESTS=ON`

### E2E (`tests/e2e/`)

- Drive real `bnetd` via real `bnbot`/`bnchat`/`bnftp`/`bnstat`
- **`nc -z` is forbidden** as a readiness probe
- Opt-in: `PVPGN_V3_E2E_TESTS=ON`

### Fuzz (`tests/fuzz/`)

- libFuzzer targets for each protocol decoder
- Seed corpora in `tests/fuzz/corpus/<target>/`
- Opt-in: `PVPGN_V3_FUZZ_TESTS=ON`

## Running Tests

```bash
# Unit + functional (default)
cmake --preset v3-dev
cmake --build --preset v3-dev
ctest --preset v3-dev

# With sanitizers
cmake --preset v3-asan
cmake --build --preset v3-asan
ctest --preset v3-asan

# Integration tests
cmake --preset v3-dev -DPVPGN_V3_INTEGRATION_TESTS=ON
ctest --preset v3-dev -L integration

# Coverage report
cmake --preset v3-coverage
cmake --build --preset v3-coverage
ctest --preset v3-coverage
```

## Catch2 Conventions

- Test names must be ASCII-only (`ctest` filter is not Unicode-safe)
- For `vector<std::byte>` equality: use a byte-loop or `static_cast<uint8_t>`; never direct `==`
- In `tests/unit/infra/legacy_config/*`: cast `StatusCode` via `static_cast<int>`
- In `tests/unit/application/anongame_inforeply/*`: direct `core::StatusCode` comparisons are OK

## Fuzz Corpus

Seed corpus files live in `tests/fuzz/corpus/<target>/`. Each file is a raw
binary input that exercises a specific code path. Add new corpus entries when
you find an interesting input during manual testing or when a fuzzer finds a
new path.
