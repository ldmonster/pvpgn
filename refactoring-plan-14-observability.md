# 14 · Observability, Logging, Metrics, Tracing

PvPGN today uses a single text log via `eventlog()` and a UDP tracker
that pings a master server with summary stats. We add structured
logging, Prometheus metrics, opt-in distributed tracing, and a richer
health/readiness surface.

## 1. Logging

* **Library**: `spdlog` (sync default; async sink for high-volume).
* **Levels**: `trace, debug, info, warn, error, critical`.
* **Sinks**:
  * Rotating file (legacy parity).
  * Stdout (color in TTY, plain in systemd).
  * Optional `syslog` (POSIX) / Windows Event Log.
  * Optional JSON-line file consumed by Loki/ELK.
* **Mandatory context** attached via `spdlog::mdc`:
  ```
  module, account_id, session_id, client_tag, ip, fiber_id, req_id
  ```
* **Macros** in `core/logging.hpp`:
  ```cpp
  LOG_INFO ("user {name} logged in from {ip}", "alice", "10.0.0.1");
  LOG_WARN ("ladder recompute slow: took {ms}ms", 1532);
  LOG_ERROR_WITH(err, "save failed");
  ```
* **Per-module log levels** in config:
  ```toml
  [log]
  default = "info"
  modules.protocol.bnet = "debug"
  modules.scripting.lua = "warn"
  ```
* `SIGUSR2` flushes logs (legacy parity); web UI also exposes
  `POST /api/v1/server/log/rotate`.

## 2. Metrics (Prometheus)

* **Library**: `prometheus-cpp` (lightweight, header-only client).
* Exposed at `GET /metrics` on the embedded HTTP server (same port
  as the WebUI by default, or a separate port via config).
* **Standard metrics**:

  ```
  pvpgn_build_info{version, commit, compiler}                       gauge
  pvpgn_uptime_seconds                                              counter
  pvpgn_sessions_open{protocol}                                     gauge
  pvpgn_sessions_total{protocol}                                    counter
  pvpgn_logins_total{result}                                        counter
  pvpgn_packets_total{protocol, direction, kind}                    counter
  pvpgn_packet_bytes_total{protocol, direction}                     counter
  pvpgn_messages_total{kind=chat|whisper|emote}                     counter
  pvpgn_games_open                                                  gauge
  pvpgn_games_started_total{tag}                                    counter
  pvpgn_games_ended_total{tag, outcome}                             counter
  pvpgn_db_query_seconds{op, table}                                 histogram
  pvpgn_repo_cache_hits_total{repo}                                 counter
  pvpgn_fiber_pool_size                                             gauge
  pvpgn_fiber_runnable                                              gauge
  pvpgn_event_bus_published_total{event_type}                       counter
  pvpgn_event_bus_dispatch_seconds{event_type}                      histogram
  pvpgn_lua_call_seconds{plugin, hook}                              histogram
  pvpgn_lua_errors_total{plugin}                                    counter
  pvpgn_webapi_requests_total{route, status}                        counter
  pvpgn_webapi_request_seconds{route}                               histogram
  ```

* **Default Grafana dashboard** ships under `packaging/grafana/`.

## 3. Tracing (optional)

* **OpenTelemetry C++ SDK**, OTLP/HTTP exporter.
* Enabled via config (`[tracing] enabled = true; endpoint =
  "http://otel-collector:4318"`).
* Each inbound packet starts a span; child spans cover repo calls,
  Lua hooks, outbound packets. Cross-service spans link bnetd ↔ d2cs ↔
  d2dbs through a propagated trace ID in the peer protocols.
* Sampling: head-based, default 1 %, exposed in config.

## 4. Health / readiness

```
GET /healthz   → 200 if process is alive (always returns)
GET /readyz    → 200 only if:
                  - all configured repositories report healthy,
                  - acceptors are listening,
                  - migrations applied,
                  - peer links (d2cs↔bnetd) connected.
GET /api/v1/server/info → {version, uptime, online, build}
```

## 5. Audit log

* Every state-changing application use-case publishes an
  `AuditEvent` to `IEventBus`:
  ```
  who:       AccountId
  action:    "ban_account" | "set_topic" | "reload_config" | …
  target:    optional resource ID
  metadata:  free-form JSON
  at:        timestamp
  ```
* Subscribers persist them (`infrastructure/observability/audit_sink.cpp`)
  to a dedicated rotating audit file and (when configured) the
  database `audit_events` table.
* Surfaced in the WebUI (`/audit`) with filtering.

## 6. Crash diagnostics

* `boost::stacktrace` linked in; on signal-fatal, a minidump-like
  textual stack is written to `${state_dir}/crashes/`.
* Optional Sentry-compatible exporter (`sentry-native`) — disabled by
  default.

## 7. Deprecated legacy observability

| Legacy | Replacement |
|---|---|
| `bnetd/tracker.cpp` UDP master tracker | Optional plug-in
  `infrastructure/observability/legacy_tracker.cpp` — emits the same
  UDP packet from a metrics subscriber. Default off. |
| `prefs_get_track_servers`, `track_users` | Removed; metrics
  exporter replaces them. |
| `eventlog_get/set_level` | Replaced by per-module spdlog config. |
| `tick.cpp` "tick" counters | Subsumed by metrics counters. |

## 8. Configuration knobs

```toml
[log]
default = "info"
file    = "/var/log/pvpgn/bnetd.log"
rotate.size_mb = 100
rotate.keep    = 10
stdout  = true
syslog  = false
json    = false

[metrics]
enabled = true
listen  = "0.0.0.0:9100"
include_default_collectors = true

[tracing]
enabled = false
service_name = "pvpgn-bnetd"
endpoint = "http://otel-collector:4318"
sample_ratio = 0.01

[audit]
file = "/var/log/pvpgn/audit.log"
to_db = true
```

## 9. SLO/SLI suggestions (ops guide)

* **Login success rate** > 99.5 % over 5 min.
* **Chat broadcast P95 latency** < 50 ms.
* **DB query P95 latency** < 25 ms.
* **Active sessions** < configured capacity × 0.8.

These are encoded as example Prometheus alerts under
`packaging/prometheus/`.
