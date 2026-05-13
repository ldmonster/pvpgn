# 11 · Testing Strategy

The legacy repository ships **two unit tests** (`bnetsrp3_test`,
`bigint`) for ~143 kLOC. Building confidence in a refactor of this
magnitude requires a layered test pyramid.

## 1. Pyramid

```
          ┌──────────────┐
          │  e2e (few)   │  docker-compose: bnetd + clients + DB
          ├──────────────┤
          │ integration  │  spin runtime with in-memory adapters
          ├──────────────┤
          │ component    │  one bounded context end-to-end
          ├──────────────┤
          │   unit       │  domain + application (majority)
          └──────────────┘
fuzz tests run continuously against codecs and parsers
property tests cover ladder math, FSM transitions
```

## 2. Frameworks

| Layer | Tooling |
|---|---|
| Unit / component / integration | **Catch2 v3** (header-light, sections, generators). |
| Mocks | **trompeloeil** (lightweight, header-only). |
| Property-based | **rapidcheck**. |
| Fuzzing | **libFuzzer** (clang-built `-fsanitize=fuzzer`) + **AFL++**. OSS-Fuzz integration as a future step. |
| Coverage | `llvm-cov` / `gcovr`, report to Codecov. |
| Benchmark | **Catch2 micro** + `google/benchmark` for hot paths. |
| Static analysis | `clang-tidy`, `cppcheck`, `iwyu` enforced in CI. |
| Sanitizers | `-fsanitize=address,undefined,thread` matrices in CI. |

## 3. Layout

```
tests/
├── unit/
│   ├── domain/
│   │   ├── identity_test.cpp
│   │   ├── chat_test.cpp
│   │   ├── gameplay_test.cpp
│   │   ├── ladder_calc_test.cpp
│   │   └── …
│   ├── application/
│   │   ├── login_user_test.cpp
│   │   ├── join_channel_test.cpp
│   │   └── …
│   ├── protocol/
│   │   ├── bnet_codec_test.cpp
│   │   ├── irc_codec_test.cpp
│   │   ├── wol_codec_test.cpp
│   │   └── d2cs_codec_test.cpp
│   └── core/
├── integration/
│   ├── auth_flow_test.cpp           # uses InMemory repos, real FSM
│   ├── chat_flow_test.cpp
│   ├── game_flow_test.cpp
│   ├── d2cs_d2dbs_test.cpp          # peer link in-process
│   ├── webapi/
│   │   ├── admin_routes_test.cpp
│   │   └── auth_routes_test.cpp
│   └── scripting/
│       └── lua_event_test.cpp
├── protocol_replay/
│   ├── traces/                      # sanitized pcaps
│   └── replay_runner.cpp
├── fuzz/
│   ├── bnet_codec_fuzz.cpp
│   ├── irc_codec_fuzz.cpp
│   ├── d2cs_codec_fuzz.cpp
│   ├── d2save_codec_fuzz.cpp
│   └── http_request_fuzz.cpp
├── e2e/
│   ├── docker-compose.yml
│   ├── scenarios/
│   │   ├── starcraft_login.py
│   │   ├── warcraft3_ladder.py
│   │   └── diablo2_realm.py
│   └── conftest.py                  # pytest fixtures
└── bench/
    ├── channel_broadcast_bench.cpp
    └── ladder_recompute_bench.cpp
```

## 4. Testing principles

* **Domain tests are pure**: no fixtures, no I/O, ms runtimes.
* **Application tests use in-memory adapters only.** Each in-memory
  repository (`InMemoryAccountRepository`, etc.) lives in
  `tests/support/` and is shared.
* **Protocol tests round-trip**: `decode(encode(msg)) == msg`,
  property-tested against generators.
* **Integration tests run the real runtime** with `InMemoryIoRuntime`
  (no real sockets) + virtual `ManualClock` — fully deterministic.
* **Replay tests** drive captured `.bin` byte streams from real
  clients through the FSM and assert on outgoing packets via golden
  files. Updating a golden file requires a code-reviewable diff.

## 5. Fuzzing

* Every codec exposes `decode(span<byte>) -> expected<Message,Error>`
  and is fuzzed independently.
* The web API request parser is fuzzed (auth bypass, integer overflow,
  path traversal).
* The Lua sandbox is fuzzed with random scripts to verify the
  capability gate.
* Found crashes go into the corpus as regression tests.

## 6. Mutation testing (stretch)

* `mull` or `pitest`-style on domain code, opt-in, run weekly.

## 7. Performance benchmarks

* Captured baselines for: channel broadcast, ladder recompute, account
  load, packet decode throughput, fiber spawn/teardown.
* CI fails if a perf-marked test regresses >10 %.

## 8. Deterministic time and randomness

* Inject `IClock` and `IRng` everywhere; tests use seeded fixtures.
* Scenarios involving `gettimeofday` or `std::random_device` directly
  are forbidden by a clang-tidy custom check.

## 9. CI matrix

```
OS:          ubuntu-22.04, ubuntu-24.04, macos-14, windows-2022
Compilers:   gcc-12, gcc-13, clang-17, clang-18, msvc-19.40
Modes:       release, debug, asan+ubsan, tsan, coverage
Backends:    sqlite (always), mysql (linux), postgres (linux)
Fuzz:        clang+fuzzer (linux, nightly)
```

The fast PR matrix is a subset (gcc-13 + clang-17 × release + asan,
SQLite only). Heavy matrix runs on `main` post-merge.

## 10. Acceptance gate for refactor PRs

Every PR that migrates a legacy module must:

1. Add or extend tests in `tests/` covering the new behavior.
2. Keep `--use-legacy=<module>` config switch working (parity gate).
3. Pass a contract test that compares legacy vs new outputs for
   recorded inputs (protocol replays, account-attribute snapshots).
