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

## CI Gates (Plan 10)

The `CI` workflow (`.github/workflows/ci.yml`) runs on every PR and enforces:

| Job | What it gates | Mechanism |
|-----|---------------|-----------|
| `lint` | unit-test pairing, no legacy/backend linkage in `tests/unit/`, no orphan scripts | `scripts/dev/check-unit-pairing.sh`, `check-test-legacy-linkage.sh`, `check-scripts-orphans.sh` |
| `layer-check` | hexagonal layering | `Dockerfile.v3 --target v3-layer-check` |
| `build-test` | full v3 build + unit suite (SQLite enabled) | `Dockerfile.v3 --target v3-test` |
| `compiler-matrix` | v3 builds + tests clean on the C++23 floor across frontends | GCC 14 + Clang 18 via the `v3-dev` preset (Plan 09) |
| `sanitizers` | ASan / UBSan / TSan clean | presets `v3-asan` / `v3-ubsan` / `v3-tsan` |
| `coverage` | `domain/` + `application/` line coverage ≥ floor | `scripts/dev/check-coverage.sh` over `v3-coverage` |
| `fuzz-smoke` | each libFuzzer harness survives a 60 s run on its corpus | preset `v3-fuzz` |

### Coverage gate

`scripts/dev/check-coverage.sh [build-dir] [floor%]` aggregates **gcov** line
coverage for files under `src/domain/` and `src/application/` (weighting each
file by its line count) and fails below the floor. It needs only `gcov` (no
gcovr/lcov), so it runs anywhere:

```bash
cmake --preset v3-coverage && cmake --build --preset v3-coverage
ctest --preset v3-coverage            # emits .gcda
scripts/dev/check-coverage.sh build/v3-coverage 60
```

**Coverage floor:** the CI job currently sets `COVERAGE_FLOOR=60` as a
**first-run calibration** value. After the first green coverage run, raise it to
just under the measured number (the long-term target is ≥ 80%, per the Unit
layer rule above) so it ratchets and catches regressions.

> The `ci.yml` apt dependency set and the coverage floor are the two first-run
> calibration points; everything else reuses the proven `Dockerfile.v3` stages
> and the existing CMake presets.

### Mutation pilot (weekly, informational)

`.github/workflows/mutation.yml` runs **weekly** (Mondays) and on demand. It is
a *pilot, not a gate* (Plan 10 step 7): it never fails the build, it publishes a
report. Rather than `mull` (which needs a bespoke LLVM/clang IR plugin
toolchain), it uses an in-tree, dependency-free equivalent:

```bash
# Mutate domain/identity/, rebuild + run the paired tests for each mutant.
python3 scripts/dev/mutation_pilot.py --build build [--max-mutants N] [--json report.json]
```

For each operator-swap mutant (`==`↔`!=`, `<=`→`<`, `>=`→`>`, `&&`↔`||`) in a
target source it rebuilds the paired test and runs it:

- test **fails** → mutant **killed** (the suite caught the change) — good;
- test **passes** → mutant **survived** — a behaviour change no test detects,
  i.e. a concrete "add a test here" pointer;
- doesn't compile → counted as killed-by-compile.

The output is a **mutation score** plus the file:line of every survivor. The
pilot is scoped to `domain/identity/` and skips operators inside comments and
string/char literals. Treat surviving mutants as a backlog of missing
assertions, not as failures.

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
