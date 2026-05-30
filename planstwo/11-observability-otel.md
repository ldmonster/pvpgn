# 11 — Observability: Real OpenTelemetry

## What

Replace the in-memory metrics registry and tracing stub with an
OpenTelemetry-compatible exporter. Logs, metrics, and traces share a
single OTLP endpoint configurable via `bnetd.toml`.

## Why

- Wave one shipped the surface (`/metrics`, `/healthz`, `/readyz`,
  `JsonLineLogger`) but no remote export. Operators have to scrape
  Prometheus locally and have no traces at all.
- Distributed tracing across `bnetd ↔ d2cs ↔ d2dbs` will be
  invaluable during the strangler completion (plans 03, 04).

## Prerequisites

- `core::IMetricsRegistry` and `core::log` already exist (wave one).
- Plan 06 lands first if at all possible — tracing async work without
  a real executor is painful.

## Concrete steps

1. **ADR `0010-otel-exporter.md`** picking between:
   - opentelemetry-cpp SDK (full-featured, large dep).
   - in-tree minimal OTLP/HTTP exporter (small, JSON-only).

   Recommendation: in-tree minimal exporter for metrics + logs; pull
   in opentelemetry-cpp only for traces (or write a minimal protobuf
   client if dep cost is unacceptable).
2. **Config.** Add `[observability]` to `bnetd.toml`:
   ```toml
   [observability]
   service_name   = "bnetd"
   otlp_endpoint  = "http://localhost:4318"
   sample_ratio   = 0.05
   ```
3. **Metrics exporter.** Implement `infra/observability/otlp_metrics.cpp`
   adapting `core::IMetricsRegistry`.
4. **Log exporter.** Add a `JsonLineLogger` sink that POSTs to
   `/v1/logs`. Local file sink stays as the default; OTLP is opt-in.
5. **Tracing.** Add `core/trace/span.hpp` (RAII, sampling, parent
   propagation). Instrument every application use case entry/exit
   (auto-spans via a decorator). Propagate trace IDs across
   `bnetd ↔ d2cs ↔ d2dbs` via a custom packet header (no wire-format
   break — header lives in our own internal channel).
6. **Standard metrics.** Promote the three mandatory metrics from
   wave one to a documented contract in
   `docs/operator/metrics.md`. Add per-use-case latency histogram and
   per-protocol byte counters.
7. **Dashboards.** Ship Grafana JSON in `contrib/dashboards/`.

## Acceptance criteria

- [ ] With `[observability].otlp_endpoint` set, traces, metrics, and
      logs land on a local OTel collector.
- [ ] With the endpoint unset, default behaviour matches today
      (local logs, in-memory metrics scraped via `/metrics`).
- [ ] Trace context propagates across `bnetd → d2cs → d2dbs` in an
      e2e fixture.
- [ ] `docs/operator/metrics.md` lists every emitted metric with type,
      labels, and rationale.

## Risks

- Sampling at 100% under load OOMs the exporter queue. Default 5%,
  documented cap.
- Adding tracing inside hot inner loops measurably slows them.
  Benchmark gate (plan 13) catches regressions.

## Out of scope

- Auth on the OTLP endpoint beyond bearer token (rely on collector
  side).
- Profiling exporter (pyroscope etc.).
