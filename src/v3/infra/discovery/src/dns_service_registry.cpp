#include "infra/discovery/dns_service_registry.hpp"

namespace pvpgn::infra::discovery {

DnsServiceRegistry::DnsServiceRegistry(std::string domain_suffix,
                                       std::shared_ptr<IServiceRegistry> fallback)
    : domain_suffix_(std::move(domain_suffix)), fallback_(std::move(fallback)) {}

core::Result<std::string, core::Error>
DnsServiceRegistry::register_service(ServiceEndpoint endpoint) {
    if (fallback_) {
        return fallback_->register_service(std::move(endpoint));
    }
    return core::fail(core::Error{
        core::StatusCode::Unimplemented,
        "DNS service registry does not support direct registration"
    });
}

core::Result<void, core::Error>
DnsServiceRegistry::deregister_service(const std::string& instance_id) {
    if (fallback_) {
        return fallback_->deregister_service(instance_id);
    }
    return core::fail(core::Error{
        core::StatusCode::Unimplemented,
        "DNS service registry does not support direct deregistration"
    });
}

std::vector<ServiceEndpoint>
DnsServiceRegistry::lookup(const std::string& service_name) const {
    auto dns_results = resolve_srv(service_name);

    if (fallback_) {
        auto fallback_results = fallback_->lookup(service_name);
        dns_results.insert(dns_results.end(), fallback_results.begin(), fallback_results.end());
    }

    return dns_results;
}

core::Result<ServiceEndpoint, core::Error>
DnsServiceRegistry::lookup_one(const std::string& service_name) {
    auto results = lookup(service_name);
    if (results.empty()) {
        return core::fail(core::Error{
            core::StatusCode::NotFound,
            "No endpoints found for service: " + service_name
        });
    }
    return results[0];
}

void DnsServiceRegistry::watch(const std::string& service_name, ServiceChangeCallback cb) {
    if (fallback_) {
        fallback_->watch(service_name, cb);
    }
}

core::Result<void, core::Error>
DnsServiceRegistry::heartbeat(const std::string& instance_id) {
    if (fallback_) {
        return fallback_->heartbeat(instance_id);
    }
    return core::fail(core::Error{
        core::StatusCode::Unimplemented,
        "DNS service registry does not support heartbeat"
    });
}

void DnsServiceRegistry::evict_expired() {
    if (fallback_) {
        fallback_->evict_expired();
    }
}

std::vector<ServiceEndpoint>
DnsServiceRegistry::resolve_srv(const std::string& service_name) const {
    // TODO: implement full DNS SRV resolution using getaddrinfo() with AI_CANONNAME
    // or res_query() for SRV records. For now, return empty to indicate no DNS results.
    // Real implementation would:
    // 1. Construct SRV query: _service._tcp.domain_suffix_
    // 2. Use res_query() or getaddrinfo() to resolve
    // 3. Parse results and return ServiceEndpoint objects
    return {};
}

} // namespace pvpgn::infra::discovery
