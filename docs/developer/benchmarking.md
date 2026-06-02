# Benchmarking

PvPGN v3 ships a small in-tree benchmark harness (Plan 13) so performance is
measured, not folklore. The async-I/O migration, the C++23 uplift, and the
OTel instrumentation each carry regression risk; the benchmarks give numbers to
defend or revert a change.

## Running

```sh
scripts/dev/run-bench.sh micro
```

This builds the (otherwise un-built) `bench_micro` target and runs it, printing
a table and writing `bench-results.json` stamped with the git rev and host:

```
  microbench (median of N samples, ns/op +- MAD)
  ------------------------------------------------------------
  bnet_codec_roundtrip/ping               33.29 ns  +-  0.54 ns  (200000 it x7)
  bnet_codec_roundtrip/joinchannel        68.33 ns  +-  0.37 ns  (100000 it x7)
  tag_table_lookup/parse_capability_x6    86.40 ns  +-  0.50 ns  (500000 it x7)
```

Point it at a specific build dir with `BUILD_DIR=build/v3-dev
scripts/dev/run-bench.sh micro`. The bench targets are `EXCLUDE_FROM_ALL` — they
are never built by `make all` and never registered with ctest, so the suite
adds nothing to normal builds.

## Microbench suite (`tests/bench/micro/`)

| Benchmark | Measures |
|-----------|----------|
| `bnet_codec_roundtrip/ping` | encode → frame → `decode_client` of a fixed-size packet (no heap) |
| `bnet_codec_roundtrip/joinchannel` | the same round-trip for a packet with a string payload (one allocation) |
| `tag_table_lookup/parse_capability_x6` | 6 lookups in the Plan 09 capability flat-map (sorted `constexpr` array + binary search) |

More cases (`srp6a_handshake`, `argon2id_verify`, `metrics_increment_contended`)
are planned; they depend on OpenSSL / libsodium and land with those backends.

## Methodology

The harness (`microbench.hpp`) is a dependency-free stand-in for `nanobench`:
for each case it runs one discarded warm-up sample, then N samples, and reports
the **median** ns/op with its **median absolute deviation (MAD)**. The plan's
risk note mandates median-of-N + MAD precisely because CI runners are noisy —
the gate is read off the median, never the worst sample. `do_not_optimize()`
keeps the optimizer from eliding work whose result is unused.

Interpreting results:

- **MAD ≪ median** (e.g. < 5%) means the sample is trustworthy. The codec and
  flat-map cases sit under ~1% MAD on an idle machine.
- **Run-to-run** differences depend on machine load; compare medians from the
  same host, and prefer three runs and the median of medians for a gate.

## Baselines and the regression gate

A baseline lives under `tests/bench/baselines/<name>.json`; `bench-results.json`
itself is generated output and is git-ignored. The gate compares a fresh run
against a baseline and fails on a budgeted regression:

```sh
scripts/dev/run-bench.sh micro      # writes bench-results.json
scripts/dev/check-bench-regression.py \
    tests/bench/baselines/local-gcc13.json bench-results.json --budget 10
```

The comparison is on the **median** ns/op (not the worst sample). It reports
each benchmark's delta and exits non-zero if any exceeds the budget.

**Host-specificity.** Absolute ns/op depend on the machine, so a baseline is
only meaningful against runs on the *same* runner. The committed
`local-gcc13.json` was captured on a developer box; the CI `microbench` job is
therefore **informational (`continue-on-error`) until the baseline is
recaptured on the CI runner** — the first CI run regenerates it, you commit that
as the baseline, then flip the job to a hard gate. (This is the same "first-run
calibration" posture as the coverage floor.)

> Policy: if you change a microbench so it "passes", justify the change in the
> PR description — a moved goalpost is not a fix.
