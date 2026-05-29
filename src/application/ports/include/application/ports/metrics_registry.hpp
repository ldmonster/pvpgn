// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file metrics_registry.hpp
/// Port for Prometheus-compatible metrics collection.
/// All metric names follow the format: pvpgn_<subsystem>_<name>_<unit>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pvpgn::application::ports {

/// Thread-safe counter metric (monotonic increasing).
class ICounter {
public:
    virtual ~ICounter() = default;
    
    /// Increment the counter by the given amount (default 1.0).
    virtual void increment(double amount = 1.0) = 0;
    
    /// Get the current counter value.
    virtual double value() const = 0;
};

/// Thread-safe gauge metric (can go up and down).
class IGauge {
public:
    virtual ~IGauge() = default;
    
    /// Set the gauge to an absolute value.
    virtual void set(double value) = 0;
    
    /// Increment the gauge by the given amount.
    virtual void increment(double amount = 1.0) = 0;
    
    /// Decrement the gauge by the given amount.
    virtual void decrement(double amount = 1.0) = 0;
    
    /// Get the current gauge value.
    virtual double value() const = 0;
};

/// Histogram metric for measuring distributions of values.
class IHistogram {
public:
    virtual ~IHistogram() = default;
    
    /// Record an observation in the histogram.
    virtual void observe(double value) = 0;
};

/// Key-value pairs for metric labels (e.g., protocol="bnet").
struct MetricLabels {
    /// List of (label_name, label_value) pairs.
    std::vector<std::pair<std::string, std::string>> pairs;
};

/// Registry for creating and managing Prometheus metrics.
/// Implements idempotent metric registration and Prometheus text format serialization.
class IMetricsRegistry {
public:
    virtual ~IMetricsRegistry() = default;
    
    /// Register or retrieve a counter metric (idempotent).
    /// If a metric with the same name already exists, returns the existing instance.
    /// @param name Prometheus metric name (e.g., pvpgn_net_packets_received_total)
    /// @param help Human-readable description
    /// @param labels Optional key-value label pairs
    /// @return Shared pointer to the counter metric
    virtual std::shared_ptr<ICounter> counter(std::string_view name, 
                                               std::string_view help,
                                               MetricLabels labels = {}) = 0;
    
    /// Register or retrieve a gauge metric (idempotent).
    /// @param name Prometheus metric name (e.g., pvpgn_net_connections_active)
    /// @param help Human-readable description
    /// @param labels Optional key-value label pairs
    /// @return Shared pointer to the gauge metric
    virtual std::shared_ptr<IGauge> gauge(std::string_view name,
                                           std::string_view help,
                                           MetricLabels labels = {}) = 0;
    
    /// Register or retrieve a histogram metric (idempotent).
    /// @param name Prometheus metric name (e.g., pvpgn_request_latency_ms)
    /// @param help Human-readable description
    /// @param buckets Optional custom bucket boundaries (will use defaults if empty)
    /// @param labels Optional key-value label pairs
    /// @return Shared pointer to the histogram metric
    virtual std::shared_ptr<IHistogram> histogram(std::string_view name,
                                                   std::string_view help,
                                                   std::vector<double> buckets = {},
                                                   MetricLabels labels = {}) = 0;
    
    /// Serialize all registered metrics to Prometheus text format.
    /// Format includes # HELP and # TYPE lines, followed by metric lines.
    /// @return Prometheus-formatted text representation
    virtual std::string serialize() const = 0;
};

}  // namespace pvpgn::application::ports
