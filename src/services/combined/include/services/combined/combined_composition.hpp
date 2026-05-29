#pragma once
// Single-binary composition root: runs bnetd + d2cs + d2dbs in one process.
// Each service runs in its own Boost.Fiber-based execution context.
// They communicate via the shared InMemoryServiceRegistry.
//
// Enable with: cmake -DPVPGN_SINGLE_BINARY=ON

#include "runtime/service_host.hpp"
#include "infra/discovery/service_registry.hpp"
#include <memory>

namespace pvpgn::services::combined {

/// CombinedComposition — wires all three services into a single process.
///
/// Architecture:
///   main() → CombinedComposition::run()
///     ├── InMemoryServiceRegistry (shared)
///     ├── BnetdComposition (bnetd service)
///     ├── D2csComposition  (d2cs service)
///     └── D2dbsComposition (d2dbs service)
///
/// Each service registers its endpoints in the shared registry on startup.
/// Inter-service calls use the registry for endpoint discovery.
class CombinedComposition : public runtime::IServiceComposition {
public:
    explicit CombinedComposition(runtime::ServiceConfig config);
    ~CombinedComposition() override = default;

    /// Get service name
    std::string service_name() const override;

    /// Initialize all sub-services and the shared registry.
    core::Result<void, std::string> init(const runtime::ServiceConfig& config) override;

    /// Start all services (each in its own fiber/thread).
    core::Result<void, std::string> start() override;

    /// Gracefully stop all services.
    void stop() override;

    /// Shutdown all services.
    void shutdown() override;

    /// Get service status
    std::string status() const override;

    /// Health check: all services must be healthy.
    [[nodiscard]] bool is_healthy() const noexcept;

    /// Get the shared service registry (for testing/inspection).
    [[nodiscard]] infra::discovery::IServiceRegistry& registry() noexcept;

private:
    runtime::ServiceConfig config_;
    std::shared_ptr<infra::discovery::InMemoryServiceRegistry> registry_;

    // Sub-service state
    bool bnetd_running_{false};
    bool d2cs_running_{false};
    bool d2dbs_running_{false};
};

} // namespace pvpgn::services::combined
