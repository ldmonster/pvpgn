// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/net/shutdown_coordinator.hpp"

#include <thread>

#include "application/persistence/unit_of_work.hpp"
#include "domain/identity/ports.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::net {

void ShutdownCoordinator::initiate_shutdown(std::chrono::seconds grace_period) {
    if (shutting_down_.exchange(true, std::memory_order_acq_rel)) {
        return;  // Already shutting down
    }

    // Step 1: Stop accepting new connections (would be done by listener)
    
    // Step 2: Notify all active sessions
    if (registry_) {
        auto sessions = registry_->list();
        // TODO: Send shutdown notification to all sessions via ISessionContext
        (void)sessions;
    }

    // Step 3: Wait for sessions to close (with timeout)
    std::this_thread::sleep_for(grace_period);

    // Step 4: Force-close remaining sessions
    if (registry_) {
        auto sessions = registry_->list();
        for (auto sid : sessions) {
            registry_->detach(sid);
        }
    }

    // Step 5: Flush all repositories via UnitOfWork
    if (uow_factory_) {
        // TODO: Create UoW and call SaveAll on each repository
        (void)uow_factory_;
    }

    // Step 6: Stop IoRuntime
    runtime_.stop();
}

}  // namespace pvpgn::infra::net
