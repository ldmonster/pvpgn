// SPDX-License-Identifier: GPL-2.0-or-later

#include "infra/net/shutdown_coordinator.hpp"

#include <thread>

#include "application/ports/unit_of_work.hpp"
#include "application/ports/session_registry.hpp"
#include "infra/net/io_runtime.hpp"

namespace pvpgn::infra::net {

void ShutdownCoordinator::initiate_shutdown(std::chrono::seconds grace_period) {
    if (shutting_down_.exchange(true, std::memory_order_acq_rel)) {
        return;  // Already shutting down
    }

    // Step 1: Stop accepting new connections (would be done by listener)
    
    // Step 2: Notify all active sessions
    if (auto registry = registry_.lock()) {
        auto sessions = registry->list();
        // TODO: Send shutdown notification to all sessions via ISessionContext
        (void)sessions;
    }

    // Step 3: Wait for sessions to close (with timeout)
    std::this_thread::sleep_for(grace_period);

    // Step 4: Force-close remaining sessions
    if (auto registry = registry_.lock()) {
        auto sessions = registry->list();
        for (auto sid : sessions) {
            registry->detach(sid);
        }
    }

    // Step 5: Flush all repositories via UnitOfWork
    if (auto uow_factory = uow_factory_.lock()) {
        // TODO: Create UoW and call SaveAll on each repository
        (void)uow_factory;
    }

    // Step 6: Stop IoRuntime
    runtime_.stop();
}

}  // namespace pvpgn::infra::net
