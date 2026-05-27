# 10 — Observability

**Goal:** A running pvpgn cluster (bnetd + d2cs + d2dbs) is
inspectable in production through three orthogonal channels:
**metrics**, **traces**, **healthchecks**. All three are
opt-in, off by default, zero-cost when disabled.

## 1. Metrics

- Adapter: `infra/metrics/prometheus/` exposes a `/metrics`
  endpoint on a configurable port (default disabled).
- Port: `MetricSink` in `application/<bc>/ports/MetricSink.h`.
  Counter / Gauge / Histogram are the only three families.
- Cardinality discipline: labels are bounded sets only — no
  per-user, per-game, or per-channel labels. Username/game-name in a
  label is forbidden by code review.

### Canonical metrics

| Name | Type | Labels | Description |
|------|------|--------|-------------|
| `pvpgn_connections_total` | counter | `protocol`, `result` | Accept results. |
| `pvpgn_connections_active` | gauge | `protocol` | Currently open. |
| `pvpgn_login_attempts_total` | counter | `result` (ok/bad_password/banned/throttled) | |
| `pvpgn_messages_sent_total` | counter | `kind` (whisper/channel/announce) | |
| `pvpgn_packets_decoded_total` | counter | `protocol`, `opcode`, `result` (ok/error) | |
| `pvpgn_games_active` | gauge | `client_type` | |
| `pvpgn_db_queries_total` | counter | `repo`, `op`, `result` | |
| `pvpgn_db_query_duration_seconds` | histogram | `repo`, `op` | |
| `pvpgn_event_loop_lag_seconds` | histogram | `loop` | scheduler latency |
| `pvpgn_build_info` | gauge=1 | `version`, `commit`, `build_type` | Static. |

## 2. Tracing

- Adapter: `infra/tracing/otlp/` exports OTLP/HTTP.
- Port: `TraceSink`. RAII `core::trace::Span`.
- Spans wrap: each use-case invocation, each DB call, each script
  invocation, each external network call.
- Default off. Sampling at 1 % when on. Tail-based sampling not in
  scope (YAGNI).

## 3. Healthchecks

Each daemon exposes `/healthz` and `/readyz` (HTTP) on the same
admin port as `/metrics`:

- `/healthz` — process alive. Returns 200 unconditionally if the
  HTTP server can serve.
- `/readyz` — daemon is ready to accept user traffic. Returns 200
  iff:
  - DB ping OK,
  - listener bound,
  - script subsystem initialised (if enabled),
  - s2s peers reachable (for bnetd: at least one d2cs registered if
    realms are configured; for d2cs: d2dbs reachable).

Used by `docker-compose` healthchecks and k8s readiness probes.

## 4. Admin port

A single HTTP server in `infra/webui/admin/`:

- `/metrics` (Prometheus exposition).
- `/healthz`, `/readyz`.
- `/version` (build info).
- `/threads` (pprof-style thread dump in debug builds only).
- `/config/effective` (redacted, behind admin auth).

The admin port is **always** bound to `127.0.0.1` by default. Binding
to a public interface requires an explicit `admin.allow_public =
true` in TOML AND a static bearer token.

## 5. Logging x tracing correlation

Every log line auto-includes the active span id and trace id (if
tracing is enabled) via a context-local lookup
(`core::log::context::current()`). KV pairs `trace_id` and `span_id`
are reserved.

## 6. Concrete tasks

- [ ] R281: define ports + `core::trace::Span`.
- [ ] R282: Prometheus adapter + admin HTTP server.
- [ ] R283: instrument repository ports (DB metrics + spans).
- [ ] R284: instrument use cases (one span per call).
- [ ] R285: OTLP exporter adapter behind a CMake option
      `PVPGN_WITH_OTLP=OFF` (default off — pulls in protobuf/grpc;
      keep optional to honour the "zero-cost when disabled" rule).
- [ ] R286: docs page `docs/observability.md` (operator-facing).
