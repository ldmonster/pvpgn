# Metrics Reference

This page is the **contract** for every metric PvPGN v3 emits: its name, type,
labels, and what it means. Metrics are registered once in
`src/infra/metrics/src/server_metrics.cpp` (`ServerMetrics::create`) against the
`core::IMetricsRegistry`.

- **Local scrape:** all metrics are exposed in Prometheus text format at
  `GET /metrics` (always on).
- **Remote export:** when `[observability].otlp_endpoint` is set
  (see [ADR 0010](../adr/0010-otel-exporter.md)), the same metrics are also
  pushed over OTLP/HTTP. With the endpoint unset, only the local scrape is
  available — behaviour is unchanged from earlier releases.

Naming follows Prometheus conventions: `pvpgn_<subsystem>_<thing>[_unit]`,
counters end in `_total`, and units are explicit in the name (`_bytes`, `_ms`).

## Mandatory contract (stable)

These three are the **stability contract** — one of each metric type, the
minimum signal an operator needs for liveness, success rate, and latency. They
will not be renamed or removed without a major-version deprecation cycle:

| Metric | Type | Meaning |
|--------|------|---------|
| `pvpgn_net_connections_active` | gauge | live connections — basic liveness/load |
| `pvpgn_auth_logins_total` | counter | successful logins — pair with failures for success rate |
| `pvpgn_request_latency_ms` | histogram | request latency distribution |

## Network

| Metric | Type | Labels | Rationale |
|--------|------|--------|-----------|
| `pvpgn_net_connections_active` | gauge | — | Currently active network connections. Primary load signal. |
| `pvpgn_net_connections_total` | counter | — | Total connections accepted since start. Rate = accept throughput. |
| `pvpgn_net_connections_by_protocol` | gauge | `protocol` (e.g. `bnet`) | Active connections split by protocol, to see traffic mix. |
| `pvpgn_net_bytes_received_total` | counter | — | Total bytes read from clients. Ingress bandwidth. |
| `pvpgn_net_bytes_sent_total` | counter | — | Total bytes written to clients. Egress bandwidth. |
| `pvpgn_net_packets_received_total` | counter | — | Total packets read. Pair with bytes for mean packet size. |
| `pvpgn_net_packets_sent_total` | counter | — | Total packets written. |

## Application

| Metric | Type | Labels | Rationale |
|--------|------|--------|-----------|
| `pvpgn_accounts_active` | gauge | — | Accounts currently online. |
| `pvpgn_channels_active` | gauge | — | Chat channels currently in use. |
| `pvpgn_games_active` | gauge | — | Games currently hosted/joined. |
| `pvpgn_auth_logins_total` | counter | — | Successful logins. |
| `pvpgn_auth_login_failures_total` | counter | — | Failed logins. Sustained rise ⇒ brute-force or misconfig. |

## Performance

| Metric | Type | Buckets / Labels | Rationale |
|--------|------|------------------|-----------|
| `pvpgn_request_latency_ms` | histogram | buckets (ms): 1, 5, 10, 25, 50, 100, 250, 500, 1000 | Request-handling latency distribution; drives p50/p95/p99. |

## Suggested alerts

- **Login success rate** — `rate(pvpgn_auth_logins_total[5m]) /
  (rate(pvpgn_auth_logins_total[5m]) + rate(pvpgn_auth_login_failures_total[5m]))`
  drops below a threshold.
- **Latency** — `histogram_quantile(0.99, rate(pvpgn_request_latency_ms_bucket[5m]))`
  exceeds the budget.
- **Saturation** — `pvpgn_net_connections_active` near
  `[policy].max_connections`.

## Adding a metric

Register it in `ServerMetrics::create` with a `_total`/unit-suffixed name and a
one-line help string, then **add a row to this page** — the metric contract is
reviewed alongside the code. See [ADR 0010](../adr/0010-otel-exporter.md) for
the export model and [the observability guide](../user/observability.md).
