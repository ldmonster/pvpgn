# 08 — Cross-Cutting Concerns

These concerns touch every layer; the discipline is to keep them *injected at
the edges* so the core stays pure.

## 1. Error handling

- **Expected failures** (bad login, name taken, channel full) are values:
  `std::expected<T, DomainError>` with exhaustive typed error enums per context.
  No exceptions, no `return false`, no log-and-continue.
- **Programmer errors / invariant breaches** use assertions (`core/debug`).
- **Truly exceptional infra faults** (DB down, socket error) may throw at the
  adapter boundary and are translated to a typed application error before
  crossing inward.
- One result/error vocabulary lives in `core/error`; contexts extend it with
  their own enums. No ad-hoc `int` error codes.

## 2. Logging

- A logging **port** is injected; `domain/` does not log at all;
  `application/` logs intent/outcome through the port; `infra/` adapters log
  operational detail.
- Structured fields, not interpolated prose, so logs are queryable.
- `check_domain_purity.sh` enforces "no logging in domain."

## 3. Time, randomness, identity

- **No `system_clock::now()`, no `std::rand`, no ad-hoc UUIDs in core logic.**
  A `Clock` port, an `Rng` port, and an `IdGenerator` port are injected.
- This is what makes use-cases deterministic and testable, and is enforced by
  the purity script for `domain/`.

## 4. Configuration

- Loaded once at startup into an immutable model (`core/config`), then injected.
- No module reaches a global config. Feature flags are config values passed to
  the composition root, not globals checked deep in the call stack.

## 5. Observability (metrics & tracing)

- Metrics and tracing are ports; adapters in `infra/{metrics,tracing}`.
- A single `sample_ratio` and consistent span naming across the three daemons;
  trace context propagates over the wire at the integration seam.
- Local-first: the default exporter is a no-op/stdout; OTLP export is an
  opt-in adapter (env-gated on libcurl + a collector).

## 6. Dependency injection / composition root

- The **only** place concrete types meet ports is `services/<daemon>` and
  `app/<daemon>`. Wiring is explicit constructor injection assembled top-down.
- No service locator, no global container, no `getInstance()` reached from
  business code. If a unit needs something, it takes it as a constructor
  parameter.
- The composition root is itself covered by a wiring/smoke test that boots the
  graph with real adapters and asserts it constructs and shuts down cleanly.

## 7. Concurrency model

- A documented, single concurrency model per daemon (event-loop in `runtime/`);
  shared mutable state is minimized and owned by one actor/thread.
- ThreadSanitizer preset (`v3-tsan`) runs the suite locally to catch data races;
  the domain is pure and therefore trivially thread-safe.

## 8. Tasks for this plan

1. Sweep for residual `system_clock::now()`/`std::rand`/global config reads
   outside composition roots and replace with injected ports.
2. Unify error vocabulary on `core/error` + per-context enums; remove `int`
   error codes and boolean-returning fallible functions.
3. Ensure logging/metrics/tracing are *only* reached via ports.
4. Add the composition-root wiring smoke test per daemon.

## Definition of Done

- [ ] No `system_clock::now()` / `std::rand` / global-config read in
      `domain`/`application` (purity + grep checks green).
- [ ] All fallible operations return typed results; no `int`/`bool` error codes
      remain in inner layers.
- [ ] No service locator/global container access in `domain`/`application`.
- [ ] Each daemon has a composition-root boot/shutdown smoke test.
- [ ] `v3-tsan` build runs the suite with no reported races.
