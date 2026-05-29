# R335 Checklist — `/healthz`, `/readyz`, `/version` admin endpoints

## Goal
Extend the existing `HttpMetricsServer` with Kubernetes-style health/readiness
probes and a version endpoint.  Wire `set_ready(true)` into `main.cpp` so that
`/readyz` returns `503` during startup and `200` once all listeners are up.

---

## Deliverables

- [x] **`src/v3/infra/metrics/include/infra/metrics/http_metrics_server.hpp`**
  - Added `#include <atomic>`
  - Updated doc-comment to list all routes
  - Added `void set_ready(bool ready) noexcept`
  - Added `[[nodiscard]] bool is_ready() const noexcept`
  - Added `std::atomic<bool> ready_{false}` private member

- [x] **`src/v3/infra/metrics/src/http_metrics_server.cpp`**
  - Added `#include "core/version.hpp"`
  - Added `PVPGN_GIT_HASH` macro guard (falls back to `"unknown"`)
  - Added `make_version_json()` helper (builds once, cached as `static`)
  - `HttpSession` constructor takes `const std::atomic<bool>& ready_flag`
  - Routing in `parse_and_respond()`:
    - `GET /healthz` → `send_json(200, {"status":"ok"})`
    - `GET /readyz`  → `200 {"status":"ready"}` or `503 {"status":"starting"}`
    - `GET /version` → static version JSON
    - `GET /config/effective` → stub JSON
  - Added `send_json(int status_code, const std::string& body)`
  - Added `send_raw(std::string data)` (keeps buffer alive via `shared_ptr`)
  - `HttpMetricsServer::Impl` takes `const std::atomic<bool>& ready_flag`
  - `HttpMetricsServer::set_ready()` / `is_ready()` implementations

- [x] **`src/v3/app/bnetd/include/app/bnetd/server_config.hpp`**
  - Added `std::uint16_t admin_port{9090}` field

- [x] **`src/v3/app/bnetd/src/main.cpp`**
  - Added conditional include block for metrics server headers:
    ```cpp
    #if __has_include("infra/metrics/http_metrics_server.hpp")
    #  include "infra/metrics/http_metrics_server.hpp"
    #  include "infra/metrics/in_memory_metrics_registry.hpp"
    #  define PVPGN_V3_BNETD_HAVE_METRICS_SERVER 1
    #endif
    ```
  - After all listeners start: create `InMemoryMetricsRegistry` + `HttpMetricsServer`,
    call `metrics_server.start()`, log the address
  - After all listeners start: call `metrics_server.set_ready(true)`, log readiness

---

## Constraints satisfied

- [x] SPDX header preserved on all modified files
- [x] `std::atomic<bool>` for thread-safe readiness flag
- [x] `memory_order_release` on store, `memory_order_acquire` on load
- [x] Conditional compilation via `__has_include` — no hard dependency
- [x] `/readyz` returns `503` before `set_ready(true)`, `200` after
- [x] `/healthz` always returns `200` (liveness)
- [x] `/version` returns static JSON built once at startup
- [x] `send_raw()` keeps the response buffer alive during async write
