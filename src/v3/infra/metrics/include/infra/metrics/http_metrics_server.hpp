// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file http_metrics_server.hpp
/// Simple HTTP server serving Prometheus-compatible /metrics endpoint.
///
/// Implements minimal HTTP/1.1:
/// - Accepts TCP connections on the configured bind address and port
/// - Parses "GET /metrics HTTP/1.1" requests
/// - Responds with "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n" + serialized metrics
/// - Responds with "HTTP/1.1 404 Not Found\r\n\r\n" for other paths
/// - Closes connection after response

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// Forward declarations
namespace pvpgn::application::ports {
class IMetricsRegistry;
}
namespace pvpgn::infra::net {
class IoRuntime;
}

namespace pvpgn::infra::metrics {

/// Minimal HTTP server for exposing Prometheus metrics.
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

private:
    std::string bind_address_;
    std::uint16_t port_;
    std::shared_ptr<application::ports::IMetricsRegistry> registry_;
    infra::net::IoRuntime& runtime_;
    
    class Impl;
    std::unique_ptr<Impl> impl_;

    void accept_connection();
};

}  // namespace pvpgn::infra::metrics
