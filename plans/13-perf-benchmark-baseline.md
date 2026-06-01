# 13 — Performance Benchmark Baseline

## What

Stand up a repeatable benchmark harness with a CI regression gate.
Cover: packet codec throughput, login storm, chat fan-out, persistence
write tps, idle-connection memory.

## Why

- The async I/O migration (plan 06), C++23 uplift (plan 09), and OTel
  instrumentation (plan 11) each have plausible regression risk. We
  need numbers to defend or revert.
- We currently ship no benchmarks; "fast enough" is folklore.

## Prerequisites

- None for the harness itself. Gate enforcement waits until plans
  06 and 11 are landing.

## Concrete steps

1. **Microbench harness** under `tests/bench/micro/` using
   `nanobench` (header-only, vcpkg). Targets:
   - `bnet_codec_roundtrip`
   - `tag_table_lookup`
   - `srp6a_handshake`
   - `argon2id_verify` (with parameter sweep)
   - `metrics_increment_contended`
2. **Macrobench harness** under `tests/bench/macro/`:
   - `login_storm` — N concurrent SRP logins/sec.
   - `chat_fanout` — broadcast latency p50/p99 with N listeners.
   - `persist_save_tps` — character saves per second per backend
     (plan 07).
   - `idle_memory` — RSS per 10k idle connections after plan 06.
3. **Runner.** `scripts/dev/run-bench.sh <suite>` produces a
   `bench-results.json` with `git rev`, host metadata, and timings.
4. **Baseline.** Capture baseline on tagged release, commit to
   `tests/bench/baselines/<release>.json`.
5. **CI gate.** Microbench runs per-PR with a 10% regression budget
   against `main`. Macrobench runs nightly on a dedicated runner.
6. **Dashboard.** Optional: `contrib/dashboards/bench.json` graphs
   the macrobench history (consumed via plan 11 OTLP, or via a
   committed CSV).

## Acceptance criteria

- [ ] `scripts/dev/run-bench.sh micro` and `macro` produce stable,
      reproducible results (≤ 5% variance across three runs).
- [ ] Microbench gate runs in CI; budgeted regressions fail the PR.
- [ ] Nightly macrobench writes results; alerts on > 25% regression.
- [ ] `docs/developer/benchmarking.md` documents the harness, the
      baseline release, and how to interpret results.

## Risks

- CI runners are noisy. Use median-of-3 + MAD; gate is on the
  median, not the worst.
- Microbench fixation. Add a comment policy: "if you're changing a
  microbench to make it pass, justify the change in the PR
  description."

## Out of scope

- Production-grade load testing (separate effort, ops-owned).
- Profile-guided optimization (PGO) — defer to wave three.
