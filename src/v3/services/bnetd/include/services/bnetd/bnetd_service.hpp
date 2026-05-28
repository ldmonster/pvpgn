// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnetd_service.hpp
/// Composition root for the bnetd server.
///
/// `BnetdService` owns the dependency graph for the bnetd server and
/// provides a clean `run()` / `stop()` lifecycle interface.  It is
/// constructed by `main()` after CLI parsing and infrastructure adapter
/// creation; `main()` itself becomes a thin bootstrap.
///
/// Dependency injection
/// --------------------
/// The constructor accepts the two primary port interfaces:
///   - `IUnitOfWorkFactory&`  — repository factory (in-memory, SQL, …)
///   - `IEventLoop&`          — event loop (Asio-backed in production)
///
/// Both are held as non-owning references; the caller (main) owns the
/// concrete adapter objects and must ensure they outlive `BnetdService`.
///
/// TODO(Phase3): wire real use-case context from IUnitOfWorkFactory into
/// the protocol FSMs once the application layer is complete.

#include <memory>

#include "application/ports/event_loop.hpp"
#include "application/ports/unit_of_work_factory.hpp"

namespace pvpgn::services::bnetd {

/// Composition root for the bnetd server.
///
/// Non-copyable, non-movable — owns internal state that must not be
/// transferred after construction.
class BnetdService {
public:
    /// Construct the service.
    ///
    /// @param uow_factory  Repository factory; must outlive this object.
    /// @param event_loop   Event loop implementation; must outlive this object.
    BnetdService(application::ports::IUnitOfWorkFactory& uow_factory,
                 application::ports::IEventLoop&         event_loop);

    ~BnetdService();

    // Non-copyable, non-movable
    BnetdService(const BnetdService&)            = delete;
    BnetdService& operator=(const BnetdService&) = delete;
    BnetdService(BnetdService&&)                 = delete;
    BnetdService& operator=(BnetdService&&)      = delete;

    /// Start the event loop and block until stop() is called or a fatal
    /// error occurs.  Delegates to IEventLoop::run().
    void run();

    /// Signal the event loop to stop.  Safe to call from any thread.
    /// Delegates to IEventLoop::stop().
    void stop() noexcept;

private:
    application::ports::IUnitOfWorkFactory& uow_factory_;
    application::ports::IEventLoop&         event_loop_;
};

}  // namespace pvpgn::services::bnetd
