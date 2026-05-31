// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file http_metrics_server.hpp
/// Simple HTTP server serving Prometheus-compatible /metrics endpoint plus
/// standard Kubernetes-style health/readiness/version probes.
///
/// Implements minimal HTTP/1.1:
/// - Accepts TCP connections on the configured bind address and port
/// - Parses "GET <path> HTTP/1.1" requests
/// - Routes:
///     GET /metrics          → Prometheus text format
///     GET /healthz          → 200 {"status":"ok"}  (liveness probe)
///     GET /readyz           → 200 {"status":"ready"} or 503 {"status":"starting"}
///     GET /version          → 200 {"version":"…","build_date":"…","git_hash":"…"}
///     GET /config/effective → 200 effective config JSON (secrets redacted)
/// - Responds with "HTTP/1.1 404 Not Found\r\n\r\n" for other paths
/// - Closes connection after response

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// Forward declarations
namespace pvpgn::core {
class IMetricsRegistry;
}
namespace pvpgn::application::ports {
using IMetricsRegistry = core::IMetricsRegistry;
}
namespace pvpgn::infra::net {
class IoRuntime;
}

namespace pvpgn::infra::metrics {

/// Minimal HTTP server for exposing Prometheus metrics and health probes.
class HttpMetricsServer {
public:
    /// Create and bind an HTTP metrics server.
    /// @param bind_address Address to bind to (e.g., "127.0.0.1" or "0.0.0.0")
    /// @param port Port to listen on (default 9090)
    /// @param registry Metrics registry to serve
    /// @param runtime IoRuntime instance providing the io_context
    HttpMetricsServer(std::string_view bind_address,
                      std::uint16_t port,
                      std::shared_ptr<application::ports::IMetricsRegistry> registry,
                      infra::net::IoRuntime& runtime);

    ~HttpMetricsServer();

    HttpMetricsServer(const HttpMetricsServer&) = delete;
    HttpMetricsServer& operator=(const HttpMetricsServer&) = delete;
    HttpMetricsServer(HttpMetricsServer&&) noexcept;
    HttpMetricsServer& operator=(HttpMetricsServer&&) noexcept;

    /// Start accepting connections.
    /// Launches the async acceptor on the IoRuntime.
    void start();

    /// Stop accepting connections and close active sessions.
    /// Idempotent; safe to call multiple times.
    void stop();

    /// Signal that the server has finished startup and is ready to serve traffic.
    /// Until this is called, GET /readyz returns 503 {"status":"starting"}.
    /// After this is called with ready=true, /readyz returns 200 {"status":"ready"}.
    void set_ready(bool ready) noexcept;

    /// Returns true if set_ready(true) has been called.
    [[nodiscard]] bool is_ready() const noexcept;

private:
    std::string bind_address_;
    std::uint16_t port_;
    std::shared_ptr<application::ports::IMetricsRegistry> registry_;
    infra::net::IoRuntime& runtime_;
    std::atomic<bool> ready_{false};

    class Impl;
    std::unique_ptr<Impl> impl_;

    void accept_connection();
};

}  // namespace pvpgn::infra::metrics
