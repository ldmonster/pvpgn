// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file server_metrics.hpp
/// Centralized server metrics collection for the bnetd server.

#include <memory>

#include "application/ports/metrics_registry.hpp"

namespace pvpgn::infra::metrics {

/// Aggregates all registered server metrics.
/// Use ServerMetrics::create() to register all metrics with a registry.
struct ServerMetrics {
    // Network metrics
    std::shared_ptr<application::ports::IGauge> active_connections;
    std::shared_ptr<application::ports::ICounter> total_connections;
    std::shared_ptr<application::ports::ICounter> bytes_received;
    std::shared_ptr<application::ports::ICounter> bytes_sent;
    std::shared_ptr<application::ports::ICounter> packets_received;
    std::shared_ptr<application::ports::ICounter> packets_sent;

    // Application metrics
    std::shared_ptr<application::ports::IGauge> active_accounts;
    std::shared_ptr<application::ports::IGauge> active_channels;
    std::shared_ptr<application::ports::IGauge> active_games;
    std::shared_ptr<application::ports::ICounter> logins_total;
    std::shared_ptr<application::ports::ICounter> login_failures;

    // Protocol breakdown
    std::shared_ptr<application::ports::IGauge> connections_by_protocol;

    // Performance
    std::shared_ptr<application::ports::IHistogram> request_latency_ms;

    /// Factory function to create and register all server metrics with the given registry.
    /// @param registry The metrics registry to register metrics with
    /// @return ServerMetrics struct with all metrics registered
    static ServerMetrics create(application::ports::IMetricsRegistry& registry);
};

}  // namespace pvpgn::infra::metrics
