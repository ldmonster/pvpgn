# ADR 0010: OpenTelemetry Exporter Strategy

**Date**: 2026-06-02
**Status**: Accepted
**Deciders**: PvPGN Core Team

## Context

Plan 11 (`plans/11-observability-otel.md`) adds remote export of logs, metrics,
and traces over a single OTLP endpoint configured in `bnetd.toml`. Wave one
shipped the local surface — `core::IMetricsRegistry` (scraped via `/metrics`),
`JsonLineLogger`, and a RAII `core::trace::Span` primitive — but nothing leaves
the process. Operators have no traces and must scrape Prometheus locally.

The decision is how to export without dragging in a heavy dependency or breaking
the "endpoint unset ⇒ behaves exactly like today" contract.

Two candidates:
- **opentelemetry-cpp SDK** — full-featured, but a large dependency tree
  (Abseil, protobuf, gRPC) that inflates build time and the image.
- **In-tree minimal OTLP/HTTP exporter** — small, JSON-only, POSTs to the OTLP
  HTTP endpoints (`/v1/metrics`, `/v1/logs`, `/v1/traces`).

## Decision

1. **In-tree minimal OTLP/HTTP (JSON) exporter** for metrics and logs; the same
   approach is extended to traces. OTLP/HTTP accepts JSON payloads, so we avoid
   the protobuf/gRPC dependency entirely. opentelemetry-cpp is **not** adopted;
   if a future need (e.g. gRPC, advanced samplers) outweighs the dependency
   cost, this ADR is revisited.

2. **Opt-in, zero-cost-when-off.** Export is enabled only when
   `[observability].otlp_endpoint` is set. With it unset, the metrics registry
   stays in-memory (scraped via `/metrics`), logs stay on the local
   `JsonLineLogger` file sink, and the span sink stays the default no-op — i.e.
   behaviour is byte-for-byte today's.

3. **Tracing primitive lives in `core/trace.hpp`** (not a new SDK):
   `core::trace::Span` is a move-only RAII unit with a `SpanContext`
   (`trace_id` / `span_id` / `parent_span_id` / `sampled`). Plan 11's
   "sampling + parent propagation" requirements are implemented here:
   - **Parent propagation** — `Span(name, parent_context)` shares the parent's
     `trace_id`, links `parent_span_id`, and mints a fresh `span_id`. A remote
     `SpanContext` (decoded from an internal inter-service header) reconstructs
     the parent on the far side, so a trace spans `bnetd → d2cs → d2dbs`.
   - **Head sampling** — a process-wide ratio (`set_sample_ratio`, default
     `1.0`) decides each root's `sampled` flag; children inherit it (consistent
     sampling). The `SpanSink` fires only for sampled spans. Production sets the
     ratio from `[observability].sample_ratio` (default 0.05).
   The concrete OTLP span exporter is just a `SpanSink` implementation under
   `infra/observability/`.

4. **Config.** `[observability]` in `bnetd.toml`:
   ```toml
   [observability]
   service_name  = "bnetd"
   otlp_endpoint = "http://localhost:4318"   # unset ⇒ export disabled
   sample_ratio  = 0.05
   ```

5. **Sampling cap.** Default `sample_ratio = 0.05`; documented as the safe
   production default to keep the exporter queue bounded (see Risks in the plan).

## Consequences

- **Positive**: no heavyweight OTel SDK; export is fully opt-in with an
  unchanged default; the tracing primitive is already in `core` and unit-tested
  (parent propagation + sampling), so the OTLP adapter is a thin sink.
- **Negative**: a hand-written JSON OTLP exporter must track the OTLP/HTTP JSON
  schema; gRPC/OTLP-protobuf collectors are unsupported (HTTP/JSON only).

## Implementation status (2026-06-02)

- [x] This ADR.
- [x] `core/trace.hpp` gains a **child-span constructor** (parent propagation)
      and **head sampling** (`set_sample_ratio` / inherited `sampled`); the sink
      fires only for sampled traces. Unit-tested in
      `tests/unit/core/trace_test.cpp` (6 cases).
- [x] `[observability]` config section + loader — `ObservabilityConfig`
      (`service_name` / `otlp_endpoint` / `sample_ratio`) in `server_config.hpp`;
      `parse_observability` (ratio clamped to [0,1]); `[observability]` added to
      `conf/bnetd.toml.in` (endpoint empty ⇒ export off). Tested in
      `server_config_test.cpp` (defaults / populated / clamping).
- [ ] Wire `observability.sample_ratio` → `core::trace::set_sample_ratio` and
      install the OTLP sinks at the composition root when `otlp_endpoint` is set.
- [ ] `infra/observability/` OTLP/HTTP JSON exporters (metrics, logs, spans),
      wired only when `otlp_endpoint` is set.
- [ ] Inter-service trace-context header for `bnetd → d2cs → d2dbs`.
- [x] `docs/operator/metrics.md` — the emitted-metric contract (all 13 metrics
      from `ServerMetrics::create` with type/labels/rationale + a stable
      3-metric mandatory contract); linked from `docs/index.md` + mkdocs nav.
- [ ] Grafana dashboards in `contrib/dashboards/`.
