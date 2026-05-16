// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// Prometheus Metrics Integration
///
/// Provides service-level metrics collection and export in Prometheus text format.
/// Tracks connections, requests, and custom gauges/counters.

#include <string>
#include <string_view>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace pvpgn::runtime {

/// Service metrics collector
class ServiceMetrics {
public:
    /// Create a metrics collector for a service
    /// @param service_name Name of the service
    explicit ServiceMetrics(std::string_view service_name);
    
    /// Record a connection accepted
    void record_connection_accepted();
    
    /// Record a connection closed
    void record_connection_closed();
    
    /// Record a connection error
    void record_connection_error();
    
    /// Record a request received
    void record_request_received();
    
    /// Record a request completed
    /// @param duration Request duration in microseconds
    void record_request_completed(std::chrono::microseconds duration);
    
    /// Record a request error
    void record_request_error();
    
    /// Set a gauge value
    /// @param name Gauge name
    /// @param value Gauge value
    void set_gauge(std::string_view name, double value);
    
    /// Increment a counter
    /// @param name Counter name
    /// @param delta Amount to increment (default: 1.0)
    void increment_counter(std::string_view name, double delta = 1.0);
    
    /// Export metrics in Prometheus text format
    /// @return Prometheus-formatted metrics string
    std::string export_prometheus() const;
    
    /// Get total connections accepted
    uint64_t connections_accepted() const { return connections_accepted_.load(); }
    
    /// Get active connections
    uint64_t connections_active() const { return connections_active_.load(); }
    
    /// Get connection errors
    uint64_t connections_errors() const { return connections_errors_.load(); }
    
    /// Get total requests received
    uint64_t requests_received() const { return requests_received_.load(); }
    
    /// Get total requests completed
    uint64_t requests_completed() const { return requests_completed_.load(); }
    
    /// Get request errors
    uint64_t requests_errors() const { return requests_errors_.load(); }
    
    /// Get average request duration in microseconds
    double average_request_duration_us() const;

private:
    std::string service_name_;
    
    // Connection metrics
    std::atomic<uint64_t> connections_accepted_{0};
    std::atomic<uint64_t> connections_active_{0};
    std::atomic<uint64_t> connections_errors_{0};
    
    // Request metrics
    std::atomic<uint64_t> requests_received_{0};
    std::atomic<uint64_t> requests_completed_{0};
    std::atomic<uint64_t> requests_errors_{0};
    std::atomic<uint64_t> total_request_duration_us_{0};
    
    // Custom metrics
    mutable std::mutex custom_metrics_mutex_;
    std::unordered_map<std::string, double> gauges_;
    std::unordered_map<std::string, double> counters_;
};

} // namespace pvpgn::runtime
