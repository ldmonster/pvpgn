#include "infra/discovery/service_registry.hpp"
#include <algorithm>
#include <sstream>
#include <mutex>

namespace pvpgn::infra::discovery {

core::Result<std::string, core::Error>
InMemoryServiceRegistry::register_service(ServiceEndpoint endpoint) {
    std::unique_lock lock(mutex_);

    // Generate instance_id if empty
    if (endpoint.instance_id.empty()) {
        std::ostringstream oss;
        oss << endpoint.service_name << ":" << endpoint.host << ":" << endpoint.port;
        endpoint.instance_id = oss.str();
    }

    endpoint.registered_at = std::chrono::steady_clock::now();
    endpoint.healthy = true;

    const auto instance_id = endpoint.instance_id;
    endpoints_[instance_id] = endpoint;

    lock.unlock();
    notify_watchers(endpoint, true);

    return instance_id;
}

core::Result<void, core::Error>
InMemoryServiceRegistry::deregister_service(const std::string& instance_id) {
    std::unique_lock lock(mutex_);

    auto it = endpoints_.find(instance_id);
    if (it == endpoints_.end()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "Service instance not found: " + instance_id
        });
    }

    const auto endpoint = it->second;
    endpoints_.erase(it);

    lock.unlock();
    notify_watchers(endpoint, false);

    return {};
}

std::vector<ServiceEndpoint>
InMemoryServiceRegistry::lookup(const std::string& service_name) const {
    std::shared_lock lock(mutex_);

    std::vector<ServiceEndpoint> result;
    for (const auto& [id, ep] : endpoints_) {
        if (ep.service_name == service_name && ep.healthy) {
            result.push_back(ep);
        }
    }
    return result;
}

core::Result<ServiceEndpoint, core::Error>
InMemoryServiceRegistry::lookup_one(const std::string& service_name) {
    std::unique_lock lock(mutex_);

    std::vector<ServiceEndpoint> healthy;
    for (const auto& [id, ep] : endpoints_) {
        if (ep.service_name == service_name && ep.healthy) {
            healthy.push_back(ep);
        }
    }

    if (healthy.empty()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "No healthy endpoints found for service: " + service_name
        });
    }

    // Round-robin load balancing
    const auto idx = round_robin_counter_++ % healthy.size();
    return healthy[idx];
}

void InMemoryServiceRegistry::watch(const std::string& service_name, ServiceChangeCallback cb) {
    std::unique_lock lock(mutex_);
    watchers_[service_name].push_back(cb);
}

core::Result<void, core::Error>
InMemoryServiceRegistry::heartbeat(const std::string& instance_id) {
    std::unique_lock lock(mutex_);

    auto it = endpoints_.find(instance_id);
    if (it == endpoints_.end()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "Service instance not found: " + instance_id
        });
    }

    it->second.registered_at = std::chrono::steady_clock::now();
    it->second.healthy = true;

    return {};
}

void InMemoryServiceRegistry::evict_expired() {
    std::unique_lock lock(mutex_);

    const auto now = std::chrono::steady_clock::now();
    std::vector<std::string> to_remove;

    for (const auto& [id, ep] : endpoints_) {
        const auto elapsed = now - ep.registered_at;
        if (elapsed > ep.ttl) {
            to_remove.push_back(id);
        }
    }

    for (const auto& id : to_remove) {
        const auto ep = endpoints_[id];
        endpoints_.erase(id);
        lock.unlock();
        notify_watchers(ep, false);
        lock.lock();
    }
}

size_t InMemoryServiceRegistry::total_count() const noexcept {
    std::shared_lock lock(mutex_);
    return endpoints_.size();
}

void InMemoryServiceRegistry::notify_watchers(const ServiceEndpoint& ep, bool registered) {
    std::shared_lock lock(mutex_);

    auto it = watchers_.find(ep.service_name);
    if (it != watchers_.end()) {
        for (const auto& cb : it->second) {
            cb(ep, registered);
        }
    }
}

} // namespace pvpgn::infra::discovery
