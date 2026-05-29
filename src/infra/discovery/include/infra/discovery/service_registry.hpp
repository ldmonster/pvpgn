#pragma once
#include "service_endpoint.hpp"
#include "core/result.hpp"
#include "core/error.hpp"
#include <vector>
#include <string>
#include <functional>
#include <shared_mutex>
#include <unordered_map>

namespace pvpgn::infra::discovery {

/// Callback invoked when a service endpoint changes (registered/deregistered/health change)
using ServiceChangeCallback = std::function<void(const ServiceEndpoint&, bool /*registered*/)>;

/// IServiceRegistry — port (interface) for service discovery
class IServiceRegistry {
public:
    virtual ~IServiceRegistry() = default;

    /// Register a service endpoint. Returns the instance_id.
    [[nodiscard]] virtual core::Result<std::string, core::Error>
    register_service(ServiceEndpoint endpoint) = 0;

    /// Deregister a service by instance_id
    [[nodiscard]] virtual core::Result<void, core::Error>
    deregister_service(const std::string& instance_id) = 0;

    /// Look up all healthy endpoints for a service name
    [[nodiscard]] virtual std::vector<ServiceEndpoint>
    lookup(const std::string& service_name) const = 0;

    /// Look up a single healthy endpoint (round-robin load balancing)
    [[nodiscard]] virtual core::Result<ServiceEndpoint, core::Error>
    lookup_one(const std::string& service_name) = 0;

    /// Subscribe to changes for a service name
    virtual void watch(const std::string& service_name, ServiceChangeCallback cb) = 0;

    /// Update health status for an instance
    [[nodiscard]] virtual core::Result<void, core::Error>
    heartbeat(const std::string& instance_id) = 0;

    /// Remove expired (TTL exceeded) endpoints
    virtual void evict_expired() = 0;
};

/// InMemoryServiceRegistry — local in-process service registry.
/// Thread-safe. Used for single-binary mode and unit tests.
class InMemoryServiceRegistry final : public IServiceRegistry {
public:
    InMemoryServiceRegistry() = default;
    ~InMemoryServiceRegistry() override = default;

    [[nodiscard]] core::Result<std::string, core::Error>
    register_service(ServiceEndpoint endpoint) override;

    [[nodiscard]] core::Result<void, core::Error>
    deregister_service(const std::string& instance_id) override;

    [[nodiscard]] std::vector<ServiceEndpoint>
    lookup(const std::string& service_name) const override;

    [[nodiscard]] core::Result<ServiceEndpoint, core::Error>
    lookup_one(const std::string& service_name) override;

    void watch(const std::string& service_name, ServiceChangeCallback cb) override;

    [[nodiscard]] core::Result<void, core::Error>
    heartbeat(const std::string& instance_id) override;

    void evict_expired() override;

    /// Total number of registered endpoints (including unhealthy)
    [[nodiscard]] size_t total_count() const noexcept;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, ServiceEndpoint> endpoints_; // instance_id → endpoint
    std::unordered_map<std::string, std::vector<ServiceChangeCallback>> watchers_; // service_name → callbacks
    mutable size_t round_robin_counter_{0};

    void notify_watchers(const ServiceEndpoint& ep, bool registered);
};

} // namespace pvpgn::infra::discovery
