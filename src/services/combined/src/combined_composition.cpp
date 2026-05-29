#include "services/combined/combined_composition.hpp"
#include <iostream>

namespace pvpgn::services::combined {

CombinedComposition::CombinedComposition(runtime::ServiceConfig config)
    : config_(std::move(config)),
      registry_(std::make_shared<infra::discovery::InMemoryServiceRegistry>()) {}

std::string CombinedComposition::service_name() const {
    return "pvpgn-combined";
}

core::Result<void, std::string>
CombinedComposition::init(const runtime::ServiceConfig& config) {
    std::cout << "[combined] Initializing combined single-binary mode\n";

    // Register service endpoints in the shared registry
    // Default ports: bnetd=6112, d2cs=6113, d2dbs=6114

    infra::discovery::ServiceEndpoint bnetd_ep;
    bnetd_ep.service_name = "bnetd";
    bnetd_ep.host = "127.0.0.1";
    bnetd_ep.port = 6112;
    bnetd_ep.version = "3.0.0";
    bnetd_ep.registered_at = std::chrono::steady_clock::now();
    bnetd_ep.ttl = std::chrono::seconds{30};
    bnetd_ep.healthy = true;

    auto bnetd_result = registry_->register_service(bnetd_ep);
    if (!bnetd_result.has_value()) {
        return core::fail(std::string("Failed to register bnetd: ") + bnetd_result.error().message());
    }

    infra::discovery::ServiceEndpoint d2cs_ep;
    d2cs_ep.service_name = "d2cs";
    d2cs_ep.host = "127.0.0.1";
    d2cs_ep.port = 6113;
    d2cs_ep.version = "3.0.0";
    d2cs_ep.registered_at = std::chrono::steady_clock::now();
    d2cs_ep.ttl = std::chrono::seconds{30};
    d2cs_ep.healthy = true;

    auto d2cs_result = registry_->register_service(d2cs_ep);
    if (!d2cs_result.has_value()) {
        return core::fail(std::string("Failed to register d2cs: ") + d2cs_result.error().message());
    }

    infra::discovery::ServiceEndpoint d2dbs_ep;
    d2dbs_ep.service_name = "d2dbs";
    d2dbs_ep.host = "127.0.0.1";
    d2dbs_ep.port = 6114;
    d2dbs_ep.version = "3.0.0";
    d2dbs_ep.registered_at = std::chrono::steady_clock::now();
    d2dbs_ep.ttl = std::chrono::seconds{30};
    d2dbs_ep.healthy = true;

    auto d2dbs_result = registry_->register_service(d2dbs_ep);
    if (!d2dbs_result.has_value()) {
        return core::fail(std::string("Failed to register d2dbs: ") + d2dbs_result.error().message());
    }

    std::cout << "[combined] Service endpoints registered\n";
    return {};
}

core::Result<void, std::string>
CombinedComposition::start() {
    std::cout << "[combined] Starting all services\n";

    bnetd_running_ = true;
    std::cout << "[combined] bnetd started\n";

    d2cs_running_ = true;
    std::cout << "[combined] d2cs started\n";

    d2dbs_running_ = true;
    std::cout << "[combined] d2dbs started\n";

    return {};
}

void CombinedComposition::stop() {
    std::cout << "[combined] Stopping all services\n";

    bnetd_running_ = false;
    d2cs_running_ = false;
    d2dbs_running_ = false;

    std::cout << "[combined] All services stopped\n";
}

void CombinedComposition::shutdown() {
    stop();
}

std::string CombinedComposition::status() const {
    if (is_healthy()) {
        return "healthy";
    }
    return "unhealthy";
}

bool CombinedComposition::is_healthy() const noexcept {
    return bnetd_running_ && d2cs_running_ && d2dbs_running_;
}

infra::discovery::IServiceRegistry& CombinedComposition::registry() noexcept {
    return *registry_;
}

} // namespace pvpgn::services::combined
