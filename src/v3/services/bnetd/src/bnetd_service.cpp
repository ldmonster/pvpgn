// SPDX-License-Identifier: GPL-2.0-or-later

/// @file bnetd_service.cpp
/// Implementation of BnetdService — composition root for the bnetd server.
///
/// Current state (Phase D stub)
/// ----------------------------
/// The constructor stores the injected port references.  `run()` delegates
/// directly to `IEventLoop::run()` and `stop()` delegates to
/// `IEventLoop::stop()`.
///
/// The actual wiring of protocol FSMs, TCP listeners, session managers, and
/// use-case contexts still lives in `app/bnetd/src/main.cpp` (the legacy
/// composition root).  That code will be migrated here incrementally in
/// subsequent refactoring steps once the application-layer use-case context
/// is fully wired through `IUnitOfWorkFactory`.
///
/// TODO(Phase3): extract BnetBnftpDispatchFactory, WOL/IRC listener setup,
/// SessionManager, and AsioEventLoop wiring from main.cpp into this class.

#include "services/bnetd/bnetd_service.hpp"

namespace pvpgn::services::bnetd {

BnetdService::BnetdService(application::ports::IUnitOfWorkFactory& uow_factory,
                           application::ports::IEventLoop&         event_loop)
    : uow_factory_(uow_factory)
    , event_loop_(event_loop)
{
    // TODO(Phase3): construct and wire infrastructure adapters here:
    //   - SessionManager
    //   - BnetBnftpDispatchFactory (BnetFsm + BnftpFsm)
    //   - WolFsm listener
    //   - IrcFsm listener
    //   - LuaRuntime
    //   - Use-case context built from uow_factory_
}

BnetdService::~BnetdService() = default;

void BnetdService::run() {
    // TODO(Phase3): start TCP listeners before entering the event loop.
    event_loop_.run();
}

void BnetdService::stop() noexcept {
    event_loop_.stop();
}

}  // namespace pvpgn::services::bnetd
