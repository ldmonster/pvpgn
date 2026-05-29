# R336 Checklist — Instrument use cases with spans + Prometheus text format

## Goal
Add `PVPGN_SPAN` instrumentation to the two most critical use cases so that
every login and channel-join operation produces a trace span.  Verify that the
`/metrics` endpoint already emits correct Prometheus text format.

---

## Deliverables

- [x] **`src/v3/application/chat/src/join_channel.cpp`**
  - Added `#include "core/trace.hpp"`
  - Added `PVPGN_SPAN("JoinChannel");` as the first statement in
    `JoinChannel::execute()`

- [x] **`src/v3/application/auth/src/login_user.cpp`**
  - Added `#include "core/trace.hpp"`
  - Added `PVPGN_SPAN("LoginUser");` as the first statement in
    `LoginUser::execute()`

- [x] **Prometheus text format** (verified, no changes needed)
  - `InMemoryMetricsRegistry::serialize()` in
    `src/v3/infra/metrics/src/in_memory_metrics_registry.cpp` already emits
    correct `# HELP` / `# TYPE` lines followed by metric samples
  - `HttpMetricsServer` serves the output with
    `Content-Type: text/plain; version=0.0.4; charset=utf-8`

---

## Span coverage

| Use case       | Span name      | File |
|----------------|----------------|------|
| `JoinChannel`  | `"JoinChannel"` | `application/chat/src/join_channel.cpp` |
| `LoginUser`    | `"LoginUser"`   | `application/auth/src/login_user.cpp`   |

---

## Constraints satisfied

- [x] `PVPGN_SPAN` placed as the **first** statement in `execute()` so the
  span covers the entire use-case duration
- [x] No changes to use-case headers or CMakeLists (header-only include)
- [x] Span ends automatically when `execute()` returns (RAII)
- [x] Prometheus text format conforms to exposition format version 0.0.4
