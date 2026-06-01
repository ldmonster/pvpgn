# Runbook: Connect an OpenTelemetry Collector

This runbook explains how to configure PvPGN to export metrics, traces, and logs to an
OpenTelemetry (OTel) collector using the OTLP/HTTP protocol.

> **Note:** Full OTel support is implemented in Plan 11. Until that plan lands, PvPGN emits
> metrics via its internal `core::IMetricsRegistry` and exposes a Prometheus scrape endpoint.
> This runbook covers both the current Prometheus path and the future OTLP path.

---

## Current State: Prometheus Scrape Endpoint

PvPGN v3 exposes a Prometheus-compatible metrics endpoint at:

```
http://<host>:<metrics_port>/metrics
```

Configure the port in `bnetd.toml`:

```toml
[observability]
metrics_port = 9090          # default
metrics_path = "/metrics"    # default
```

Point your Prometheus instance at this endpoint:

```yaml
# prometheus.yml
scrape_configs:
  - job_name: pvpgn
    static_configs:
      - targets: ['pvpgn-host:9090']
    scrape_interval: 15s
```

---

## Future State: OTLP/HTTP Export (Plan 11)

After Plan 11 lands, configure the `[observability]` section in `bnetd.toml`:

```toml
[observability]
otlp_endpoint    = "http://otel-collector:4318"   # OTLP/HTTP receiver
otlp_headers     = { "x-api-key" = "env:OTEL_API_KEY" }
otlp_timeout_ms  = 5000
service_name     = "pvpgn-bnetd"
service_version  = "3.0.0"

# What to export
export_metrics   = true
export_traces    = true
export_logs      = true

# Sampling (traces only)
trace_sample_rate = 0.1   # 10% of requests
```

If `otlp_endpoint` is not set, PvPGN falls back to the Prometheus scrape endpoint and
no-op trace/log exporters. No data is lost.

---

## Docker Compose Example

A minimal stack with PvPGN, an OTel Collector, and Grafana:

```yaml
# docker-compose.yml
version: "3.9"

services:
  pvpgn:
    image: pvpgn/pvpgn:latest
    ports:
      - "6112:6112"
      - "9090:9090"
    environment:
      PVPGN_BNETD__OBSERVABILITY__OTLP_ENDPOINT: "http://otel-collector:4318"
    volumes:
      - ./bnetd.toml:/etc/pvpgn/bnetd.toml:ro
      - pvpgn-data:/var/lib/pvpgn

  otel-collector:
    image: otel/opentelemetry-collector-contrib:latest
    volumes:
      - ./otel-collector.yml:/etc/otelcol/config.yaml:ro
    ports:
      - "4318:4318"   # OTLP/HTTP
      - "8888:8888"   # Collector self-metrics

  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro
    ports:
      - "9091:9090"

  grafana:
    image: grafana/grafana:latest
    ports:
      - "3000:3000"
    environment:
      GF_SECURITY_ADMIN_PASSWORD: admin

volumes:
  pvpgn-data:
```

OTel Collector configuration (`otel-collector.yml`):

```yaml
receivers:
  otlp:
    protocols:
      http:
        endpoint: "0.0.0.0:4318"

processors:
  batch:
    timeout: 10s

exporters:
  prometheus:
    endpoint: "0.0.0.0:8889"
  logging:
    verbosity: normal

service:
  pipelines:
    metrics:
      receivers: [otlp]
      processors: [batch]
      exporters: [prometheus, logging]
    traces:
      receivers: [otlp]
      processors: [batch]
      exporters: [logging]
    logs:
      receivers: [otlp]
      processors: [batch]
      exporters: [logging]
```

---

## Key Metrics Emitted by PvPGN

| Metric | Type | Labels | Description |
|--------|------|--------|-------------|
| `pvpgn_connections_active` | Gauge | `protocol` | Active TCP connections |
| `pvpgn_logins_total` | Counter | `result` | Login attempts (success/failure) |
| `pvpgn_auth_hash_algo` | Counter | `algo` | Password hash algorithm used |
| `pvpgn_games_active` | Gauge | `game_type` | Active game lobbies |
| `pvpgn_db_query_ms` | Histogram | `table`, `op` | Database query latency |
| `pvpgn_channel_members` | Gauge | `channel` | Members per channel |

---

## Verifying the Connection

```bash
# Check that the collector is receiving data
curl -s http://otel-collector:8888/metrics | grep otelcol_receiver_accepted_metric_points

# Check PvPGN's own metrics endpoint
curl -s http://pvpgn-host:9090/metrics | grep pvpgn_connections_active
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No metrics in Grafana | Prometheus not scraping | Check `prometheus.yml` targets |
| `connection refused` on 4318 | Collector not running | `docker compose up otel-collector` |
| `otlp_endpoint` ignored | Plan 11 not yet landed | Use Prometheus scrape path |
| High cardinality warning | Too many label values | Reduce `channel` label cardinality |
