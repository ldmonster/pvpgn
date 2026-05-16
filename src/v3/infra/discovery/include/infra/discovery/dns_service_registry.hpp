#pragma once
#include "service_registry.hpp"
#include <string>
#include <memory>

namespace pvpgn::infra::discovery {

/// DnsServiceRegistry — resolves service endpoints via DNS SRV records.
/// Falls back to InMemoryServiceRegistry for local overrides.
///
/// DNS SRV record format: _service._tcp.domain
/// Example: _bnetd._tcp.pvpgn.local → host:port
class DnsServiceRegistry final : public IServiceRegistry {
public:
    explicit DnsServiceRegistry(std::string domain_suffix,
                                 std::shared_ptr<IServiceRegistry> fallback = nullptr);
    ~DnsServiceRegistry() override = default;

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

private:
    std::string domain_suffix_;
    std::shared_ptr<IServiceRegistry> fallback_;

    /// Resolve DNS SRV record for _service._tcp.domain_suffix_
    [[nodiscard]] std::vector<ServiceEndpoint>
    resolve_srv(const std::string& service_name) const;
};

} // namespace pvpgn::infra::discovery
