// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file server_metrics.hpp
/// Centralized server metrics collection for the bnetd server.

#include <memory>

#include "core/metrics.hpp"

namespace pvpgn::infra::metrics {

/// Aggregates all registered server metrics.
/// Use ServerMetrics::create() to register all metrics with a registry.
struct ServerMetrics {
    // Network metrics
    std::shared_ptr<core::IGauge> active_connections;
    std::shared_ptr<core::ICounter> total_connections;
    std::shared_ptr<core::ICounter> bytes_received;
    std::shared_ptr<core::ICounter> bytes_sent;
    std::shared_ptr<core::ICounter> packets_received;
    std::shared_ptr<core::ICounter> packets_sent;

    // Application metrics
    std::shared_ptr<core::IGauge> active_accounts;
    std::shared_ptr<core::IGauge> active_channels;
    std::shared_ptr<core::IGauge> active_games;
    std::shared_ptr<core::ICounter> logins_total;
    std::shared_ptr<core::ICounter> login_failures;

    // Protocol breakdown
    std::shared_ptr<core::IGauge> connections_by_protocol;

    // Performance
    std::shared_ptr<core::IHistogram> request_latency_ms;

    /// Factory function to create and register all server metrics with the given registry.
    /// @param registry The metrics registry to register metrics with
    /// @return ServerMetrics struct with all metrics registered
    static ServerMetrics create(core::IMetricsRegistry& registry);
};

}  // namespace pvpgn::infra::metrics
