# PvPGN v3 Observability Guide

This document describes the observability stack built into the PvPGN v3 server:
metrics, health probes, distributed tracing, and the OTLP exporter.

---

## Table of Contents

1. [Metrics — Prometheus text format](#metrics--prometheus-text-format)
2. [Health & Readiness Probes](#health--readiness-probes)
3. [Version Endpoint](#version-endpoint)
4. [Distributed Tracing](#distributed-tracing)
   - [core::trace::Span](#coretracspan)
   - [PVPGN_SPAN macro](#pvpgn_span-macro)
   - [LogTraceSink](#logtracesink)
   - [OtlpTraceSink](#otlptracesink)
5. [Configuration Reference](#configuration-reference)
6. [Kubernetes / Docker Integration](#kubernetes--docker-integration)

---

## Metrics — Prometheus text format

The admin HTTP server exposes a `/metrics` endpoint in the
[Prometheus text exposition format (version 0.0.4)](https://prometheus.io/docs/instrumenting/exposition_formats/).

**Default address:** `http://0.0.0.0:9090/metrics`

### Example output

```
# HELP pvpgn_connections_total Total TCP connections accepted
# TYPE pvpgn_connections_total counter
pvpgn_connections_total 42

# HELP pvpgn_active_sessions Current number of active sessions
# TYPE pvpgn_active_sessions gauge
pvpgn_active_sessions 7

# HELP pvpgn_login_duration_seconds Login use-case latency histogram
# TYPE pvpgn_login_duration_seconds histogram
pvpgn_login_duration_seconds_bucket{le="0.005"} 120
pvpgn_login_duration_seconds_bucket{le="0.01"} 135
pvpgn_login_duration_seconds_bucket{le="+Inf"} 140
pvpgn_login_duration_seconds_sum 0.412
pvpgn_login_duration_seconds_count 140
```

### Scraping with Prometheus

Add the following job to your `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: pvpgn
    static_configs:
      - targets: ['localhost:9090']
    scrape_interval: 15s
```

---

## Health & Readiness Probes

The admin HTTP server exposes Kubernetes-style health probes.

| Endpoint   | Method | Description                                      |
|------------|--------|--------------------------------------------------|
| `/healthz` | GET    | **Liveness** — always returns `200 {"status":"ok"}` while the process is running |
| `/readyz`  | GET    | **Readiness** — returns `200 {"status":"ready"}` after all listeners are up; `503 {"status":"starting"}` during startup |

### Kubernetes probe configuration

```yaml
livenessProbe:
  httpGet:
    path: /healthz
    port: 9090
  initialDelaySeconds: 5
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /readyz
    port: 9090
  initialDelaySeconds: 3
  periodSeconds: 5
```

---

## Version Endpoint

```
GET /version
```

Returns a JSON object with build metadata:

```json
{
  "version": "3.0.0-dev",
  "build_date": "May 29 2026",
  "git_hash": "a1b2c3d4e5f6..."
}
```

The `git_hash` field is populated at build time via the `-DPVPGN_GIT_HASH=<hash>`
CMake variable.  It defaults to `"unknown"` when not set.

---

## Distributed Tracing

PvPGN v3 uses a lightweight, zero-dependency tracing layer defined in
`core/trace.hpp`.  Spans are RAII objects that record start/end times and
key–value attributes.  A process-wide **SpanSink** callback is invoked in the
span destructor; the concrete exporter (log, OTLP, no-op) is wired at startup.

### `core::trace::Span`

```cpp
#include "core/trace.hpp"

// Create a root span — ends when the variable goes out of scope.
pvpgn::core::trace::Span span{"MyOperation"};
span.set_attribute("user", username);

// Mark success or failure explicitly (optional — default is OK).
span.set_status_ok();
span.set_status_error("something went wrong");
```

Each span carries a `SpanContext` with:
- `trace_id` — 32-char hex (16 random bytes)
- `span_id`  — 16-char hex (8 random bytes)
- `parent_span_id` — empty for root spans

### `PVPGN_SPAN` macro

The `PVPGN_SPAN(name)` macro creates a RAII span that lives until the
enclosing scope exits.  The variable name is mangled with `__LINE__` to
avoid collisions when the macro is used multiple times in the same function.

```cpp
#include "core/trace.hpp"

core::Result<JoinChannelResult, JoinChannelError>
JoinChannel::execute(domain::AccountId id, const std::string& channel) const {
    PVPGN_SPAN("JoinChannel");   // ← span ends when execute() returns
    // ...
}
```

### Registering a sink

```cpp
#include "core/trace.hpp"
#include "infra/tracing/log_trace_sink.hpp"

// In main() / composition root:
auto sink = std::make_shared<pvpgn::infra::tracing::LogTraceSink>();
pvpgn::core::trace::set_global_span_sink(
    [sink](const pvpgn::core::trace::Span& s) { sink->record(s); });
```

### LogTraceSink

Always available.  Writes each completed span as a structured `INFO` log line:

```
[trace] span=a1b2c3d4e5f6g7h8 trace=<32-char-hex> parent=- op=JoinChannel dur_us=142 status=ok
```

### OtlpTraceSink

Available when `PVPGN_V3_WITH_OTLP=ON` is passed to CMake.  Exports spans
to an [OpenTelemetry](https://opentelemetry.io/) collector via **OTLP/HTTP
(JSON)** on each `record()` call (synchronous).

```cmake
cmake -DPVPGN_V3_WITH_OTLP=ON ..
```

```cpp
#ifdef PVPGN_V3_WITH_OTLP
#include "infra/tracing/otlp_trace_sink.hpp"

auto sink = std::make_shared<pvpgn::infra::tracing::OtlpTraceSink>(
    "http://localhost:4318/v1/traces");
pvpgn::core::trace::set_global_span_sink(
    [sink](const pvpgn::core::trace::Span& s) { sink->record(s); });
#endif
```

#### Running a local collector (Jaeger all-in-one)

```bash
docker run --rm -p 4318:4318 -p 16686:16686 \
    jaegertracing/all-in-one:latest \
    --collector.otlp.enabled=true
```

Open `http://localhost:16686` to view traces in the Jaeger UI.

---

## Configuration Reference

| `ServerConfig` field | Default | Description |
|----------------------|---------|-------------|
| `admin_port`         | `9090`  | TCP port for the admin HTTP server (`/metrics`, `/healthz`, `/readyz`, `/version`) |

Set via `bnetd.toml`:

```toml
[network]
admin_port = 9090
```

Or via the `--admin-port` CLI flag (when implemented).

---

## Kubernetes / Docker Integration

### `docker-compose.v3.yml` snippet

```yaml
services:
  pvpgn:
    image: pvpgn-v3
    ports:
      - "6112:6112"   # BNet / BNFTP
      - "9090:9090"   # Admin HTTP (metrics + probes)
    healthcheck:
      test: ["CMD", "wget", "-qO-", "http://localhost:9090/healthz"]
      interval: 10s
      timeout: 3s
      retries: 3
      start_period: 5s
```

### Grafana dashboard

Import the community dashboard **PvPGN v3** (ID TBD) or create a custom
dashboard using the metrics exposed at `/metrics`.  Key panels:

- **Active sessions** — `pvpgn_active_sessions`
- **Login rate** — `rate(pvpgn_login_total[1m])`
- **Login latency p99** — `histogram_quantile(0.99, rate(pvpgn_login_duration_seconds_bucket[5m]))`
