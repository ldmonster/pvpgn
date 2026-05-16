// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// Health Check System
///
/// Provides a registry for health checks with support for running checks,
/// determining overall health status, and exporting results in JSON and Prometheus formats.

#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace pvpgn::runtime {

/// Health status enumeration
enum class HealthStatus {
    healthy,
    degraded,
    unhealthy
};

/// Result of a single health check
struct HealthCheckResult {
    /// Overall status
    HealthStatus status;
    
    /// Human-readable message
    std::string message;
    
    /// Time when check was performed
    std::chrono::system_clock::time_point checked_at;
    
    /// Additional details as key-value pairs
    std::unordered_map<std::string, std::string> details;
};

/// Health check function type
using HealthCheckFn = std::function<HealthCheckResult()>;

/// Health check registry
class HealthCheckRegistry {
public:
    /// Register a health check
    /// @param name Name of the check
    /// @param fn Health check function
    void register_check(std::string name, HealthCheckFn fn);
    
    /// Unregister a health check
    /// @param name Name of the check to remove
    void unregister_check(const std::string& name);
    
    /// Run all registered health checks
    /// @return Map of check names to results
    std::unordered_map<std::string, HealthCheckResult> run_all() const;
    
    /// Get overall health status (worst of all checks)
    /// @return Overall health status
    HealthStatus overall_status() const;
    
    /// Export health check results as JSON
    /// @return JSON string representation
    std::string export_json() const;
    
    /// Export health check results as Prometheus metrics
    /// @return Prometheus-formatted metrics
    std::string export_prometheus() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, HealthCheckFn> checks_;
};

} // namespace pvpgn::runtime
