# R337 Checklist — OTLP exporter adapter + `docs/observability.md`

## Goal
Provide concrete `ITraceSink` adapters in `infra/tracing/`:
- `LogTraceSink` — always available; writes spans to spdlog
- `OtlpTraceSink` — conditionally compiled with `PVPGN_V3_WITH_OTLP=ON`;
  exports spans via OTLP/HTTP (JSON) to an OpenTelemetry collector

Document the entire observability stack in `docs/observability.md`.

---

## Deliverables

- [x] **`src/v3/infra/tracing/include/infra/tracing/log_trace_sink.hpp`**
  - `LogTraceSink : public ITraceSink`
  - `void record(const Span&) noexcept override`

- [x] **`src/v3/infra/tracing/src/log_trace_sink.cpp`**
  - Emits `[trace] span=… trace=… parent=… op=… dur_us=… status=ok|error` via `SPDLOG_INFO`
  - Catches all exceptions (never propagates from a sink)

- [x] **`src/v3/infra/tracing/include/infra/tracing/otlp_trace_sink.hpp`**
  - `#ifdef PVPGN_V3_WITH_OTLP` guard
  - `OtlpTraceSink : public ITraceSink`
  - Constructor takes `std::string endpoint` (full OTLP/HTTP URL)
  - `void record(const Span&) noexcept override`
  - `#else` branch: comment explaining the type is unavailable

- [x] **`src/v3/infra/tracing/src/otlp_trace_sink.cpp`**
  - `#ifdef PVPGN_V3_WITH_OTLP` guard
  - `json_escape()` helper
  - `to_unix_ns()` helper
  - `build_otlp_json()` — minimal OTLP/HTTP JSON payload
  - `parse_http_url()` — splits URL into host/port/path
  - `OtlpTraceSink::record()` — synchronous Boost.Beast HTTP POST
  - Logs WARN on HTTP 4xx/5xx or exception; never throws

- [x] **`src/v3/infra/tracing/CMakeLists.txt`**
  - `option(PVPGN_V3_WITH_OTLP ...)` defaulting to `OFF`
  - `pvpgn_v3_add_library(infra_tracing ...)` with `log_trace_sink.cpp`
  - Conditionally adds `otlp_trace_sink.cpp` when `PVPGN_V3_WITH_OTLP=ON`
  - Propagates `PVPGN_V3_WITH_OTLP` compile definition publicly
  - Links `Boost::system` privately when OTLP is enabled

- [x] **`src/v3/CMakeLists.txt`** — `infra_tracing` library defined inline
  after `infra_metrics` section (same pattern as other infra libs)

- [x] **`docs/observability.md`**
  - Metrics / Prometheus text format section with example output + scrape config
  - Health & readiness probes section with Kubernetes probe YAML
  - Version endpoint section
  - Distributed tracing section:
    - `core::trace::Span` API
    - `PVPGN_SPAN` macro usage
    - Registering a sink
    - `LogTraceSink` description + example log line
    - `OtlpTraceSink` description + CMake flag + Jaeger docker-compose snippet
  - Configuration reference table
  - Kubernetes / Docker integration section

---

## Constraints satisfied

- [x] SPDX header on all new files
- [x] `#pragma once` on all headers
- [x] `OtlpTraceSink` only compiled when `PVPGN_V3_WITH_OTLP=ON`
- [x] `LogTraceSink` always available (no optional dependency)
- [x] Both sinks implement `ITraceSink` from `application/ports/trace_sink.hpp`
- [x] Both `record()` methods are `noexcept` and catch all exceptions
- [x] OTLP payload follows opentelemetry-proto JSON encoding
- [x] `docs/observability.md` covers all four R334–R337 features
