// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file shutdown_coordinator.hpp
/// Graceful shutdown coordinator.
/// Orchestrates clean server shutdown: notify sessions, flush data, stop runtime.

#include <atomic>
#include <chrono>
#include <memory>

#include "core/result.hpp"
#include "application/persistence/unit_of_work_factory.hpp"
#include "domain/identity/ports.hpp"

// Forward declarations
namespace pvpgn::infra::net {
class IoRuntime;
}
namespace pvpgn::domain::identity {
class ISessionRegistry;
}
namespace pvpgn::application::ports {
class IUnitOfWorkFactory;
using domain::identity::ISessionRegistry;
}

namespace pvpgn::infra::net {

/// Coordinates graceful shutdown sequence.
class ShutdownCoordinator {
public:
    ShutdownCoordinator(
        IoRuntime& runtime,
        std::shared_ptr<pvpgn::application::ports::IUnitOfWorkFactory> uow_factory,
        std::shared_ptr<pvpgn::domain::identity::ISessionRegistry> registry)
        : runtime_(runtime), uow_factory_(uow_factory), registry_(registry) {}

    /// Initiate graceful shutdown.
    /// 1. Stop accepting new connections
    /// 2. Notify all active sessions ("Server shutting down in Ns")
    /// 3. Wait up to grace_period for sessions to close naturally
    /// 4. Force-close remaining sessions
    /// 5. Flush all repositories via UnitOfWork::SaveAll
    /// 6. Stop IoRuntime
    ///
    /// @param grace_period Time to wait for sessions to close (default 30s)
    void initiate_shutdown(
        std::chrono::seconds grace_period = std::chrono::seconds{30});

    /// Check if shutdown is in progress.
    bool is_shutting_down() const {
        return shutting_down_.load(std::memory_order_acquire);
    }

private:
    IoRuntime& runtime_;
    std::shared_ptr<pvpgn::application::ports::IUnitOfWorkFactory> uow_factory_;
    std::shared_ptr<pvpgn::domain::identity::ISessionRegistry> registry_;
    std::atomic<bool> shutting_down_{false};
};

}  // namespace pvpgn::infra::net
