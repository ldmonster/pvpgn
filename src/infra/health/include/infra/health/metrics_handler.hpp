// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file metrics_handler.hpp
/// HTTP handler for the Prometheus scrape endpoint.
///
/// Route:
///   GET /metrics  — Prometheus text exposition format (0.0.4).
///                   Content-Type: text/plain; version=0.0.4; charset=utf-8
///
/// Like `HealthHandler`, this class is decoupled from any specific HTTP
/// server.  `handle()` returns a complete HTTP/1.1 response string.

#include <memory>
#include <string>
#include <string_view>

#include "core/metrics.hpp"

namespace pvpgn::infra::health {

/// Produces HTTP/1.1 response strings for the /metrics scrape endpoint.
class MetricsHandler {
public:
    /// @param registry  The metrics registry to serialize on each request.
    ///                  Must not be null.
    explicit MetricsHandler(
        std::shared_ptr<core::IMetricsRegistry> registry);

    /// Handle an HTTP GET /metrics request.
    ///
    /// @returns  A complete HTTP/1.1 response string with
    ///           Content-Type: text/plain; version=0.0.4; charset=utf-8
    ///           and the Prometheus text body.
    [[nodiscard]] std::string handle() const;

private:
    std::shared_ptr<core::IMetricsRegistry> registry_;

    static std::string make_response(std::string_view body);
};

}  // namespace pvpgn::infra::health
