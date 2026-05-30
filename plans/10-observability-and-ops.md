# 10 — Observability and operations

## What

Operators today have: text log file with comma-separated levels, optional Prometheus endpoint, optional OTLP traces. Tighten these into a coherent ops UX so a single dashboard answers "is bnetd healthy and why not".

## Concrete steps

### Logging

1. One library: `src/infra/log/`. Delete `src/common/eventlog.{cpp,h}` after the legacy tree goes (plan 06).
2. Structured logging by default: `key=value` pairs or JSON (operator-selectable via `[log].format = "text" | "json"`).
3. Levels normalized to `trace,debug,info,warn,error,fatal`. Remove the legacy `loglevels = "fatal,error,..."` CSV in favour of `min_level = "info"` + per-module overrides:

   ```toml
   [log]
   min_level = "info"
   format    = "json"
   [log.modules]
   "chat"        = "debug"
   "connection"  = "trace"
   ```

4. Rotation: keep current settings (`rotate_size`, `rotate_files`). Add `rotate_when = "size" | "daily"`.

### Metrics

5. Single registry: `src/infra/metrics/`. Expose on `/metrics` from the existing `webui` endpoint.
6. Metric naming: `pvpgn_<context>_<noun>_<unit>` (Prometheus convention). Document in `docs/observability.md`.
7. Mandatory metrics per context:
   - `pvpgn_<ctx>_requests_total{op,result}`
   - `pvpgn_<ctx>_request_seconds{op}` (histogram)
   - `pvpgn_<ctx>_inflight{op}`
8. Add `pvpgn_strangler_calls_total{bridge,outcome}` so plan 06 progress is observable in production.

### Tracing

9. OTLP exporter stays optional (`PVPGN_V3_WITH_OTLP=ON`).
10. Span naming: `<context>.<use_case>` (e.g. `identity.authenticate`).
11. Propagate trace IDs through the bnet protocol's `keepalive` channel-tag piggyback **only** for inter-pvpgn calls (d2cs<->bnetd), never to the client.

### Health

12. `/healthz` (liveness): always 200 if event loop is responsive.
13. `/readyz` (readiness): 200 once persistence is reachable, config validated, all required ports bound.
14. `/varz` (debug dump, opt-in): TOML snapshot, plugin list, Lua VM stats.

### Operator CLI

15. `bnetd --check-config <file>` validates without starting (already partly there).
16. `bnetd --print-effective-config` emits the merged TOML view (post-defaults).
17. `pvpgn-config` command remains the interactive editor.

## Acceptance criteria

- [ ] All log lines parse as JSON when `[log].format = "json"`.
- [ ] Every application use case emits the three mandatory metrics.
- [ ] `bnetd --check-config conf/bnetd.toml.in` exits 0 in CI.
- [ ] `/healthz`, `/readyz`, `/metrics` documented in `docs/observability.md` with sample dashboard JSON.

## Risks

- Switching log format breaks log-shipping pipelines. Default stays `text`; operators opt in.

## Out of scope

- Shipping a managed dashboard. Provide a sample Grafana JSON, nothing more.
