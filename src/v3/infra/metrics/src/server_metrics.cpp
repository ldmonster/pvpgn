// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/metrics/server_metrics.hpp"

namespace pvpgn::infra::metrics {

ServerMetrics ServerMetrics::create(application::ports::IMetricsRegistry& registry) {
    ServerMetrics metrics;

    // Network metrics
    metrics.active_connections = registry.gauge(
        "pvpgn_net_connections_active",
        "Currently active network connections");

    metrics.total_connections = registry.counter(
        "pvpgn_net_connections_total",
        "Total number of connections accepted");

    metrics.bytes_received = registry.counter(
        "pvpgn_net_bytes_received_total",
        "Total bytes received from clients");

    metrics.bytes_sent = registry.counter(
        "pvpgn_net_bytes_sent_total",
        "Total bytes sent to clients");

    metrics.packets_received = registry.counter(
        "pvpgn_net_packets_received_total",
        "Total packets received from clients");

    metrics.packets_sent = registry.counter(
        "pvpgn_net_packets_sent_total",
        "Total packets sent to clients");

    // Application metrics
    metrics.active_accounts = registry.gauge(
        "pvpgn_accounts_active",
        "Number of currently active accounts");

    metrics.active_channels = registry.gauge(
        "pvpgn_channels_active",
        "Number of currently active channels");

    metrics.active_games = registry.gauge(
        "pvpgn_games_active",
        "Number of currently active games");

    metrics.logins_total = registry.counter(
        "pvpgn_auth_logins_total",
        "Total successful logins");

    metrics.login_failures = registry.counter(
        "pvpgn_auth_login_failures_total",
        "Total login failures");

    // Protocol breakdown with labels
    application::ports::MetricLabels bnet_label;
    bnet_label.pairs.push_back({"protocol", "bnet"});
    metrics.connections_by_protocol = registry.gauge(
        "pvpgn_net_connections_by_protocol",
        "Active connections grouped by protocol",
        bnet_label);

    // Performance metrics
    std::vector<double> latency_buckets = {1.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0};
    metrics.request_latency_ms = registry.histogram(
        "pvpgn_request_latency_ms",
        "Request latency in milliseconds",
        latency_buckets);

    return metrics;
}

}  // namespace pvpgn::infra::metrics
