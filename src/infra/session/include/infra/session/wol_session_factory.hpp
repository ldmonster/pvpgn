// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_session_factory.hpp
/// SessionFactory for WOL (Warcraft Online) game server (Phase 6b).
/// Creates WolFsm for each accepted connection.

#include <atomic>
#include <cstdint>
#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "domain/identity/ports.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "infra/net/tcp_session.hpp"
#include "protocol/wolgameres/wol_fsm.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/event_bus.hpp"

namespace pvpgn::infra::session {

/// Factory that creates a WOL game session.
/// Each connection is handled by WolFsm.
class WolSessionFactory {
public:
    /// Create a factory with the given dependencies.
    WolSessionFactory(
        std::shared_ptr<domain::identity::ISessionRegistry> registry,
        std::shared_ptr<pvpgn::domain::gameplay::IGameRepository> games,
        std::shared_ptr<pvpgn::application::ports::IEventBus> event_bus)
        : registry_(registry), games_(games), event_bus_(event_bus) {}

    /// Called by TcpAcceptor for each accepted connection.
    /// Instantiates WolFsm to handle the session.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp_session) {
        if (!tcp_session) return;

        // Generate a new SessionId
        domain::SessionId session_id{next_session_id_.fetch_add(1)};

        // TODO: Create ISessionContext wrapper for tcp_session
        // For now, we instantiate the FSM but don't wire it
        // auto fsm = std::make_shared<protocol::wol::WolFsm>(
        //     session_context, games_, event_bus_);

        // Set up TCP session callbacks
        tcp_session->set_on_bytes([session_id](core::ByteView bytes) {
            // TODO: Feed bytes to WolFsm::on_bytes
            (void)bytes;
        });

        tcp_session->set_on_close([this, session_id](
                                      const boost::system::error_code& ec) {
            // Cleanup on close
            if (auto registry_ptr = registry_.lock()) {
                registry_ptr->detach(session_id);
            }
            (void)ec;
        });

        // Start reading
        tcp_session->start();
    }

private:
    std::weak_ptr<domain::identity::ISessionRegistry> registry_;
    std::shared_ptr<pvpgn::domain::gameplay::IGameRepository> games_;
    std::shared_ptr<pvpgn::application::ports::IEventBus> event_bus_;
    static std::atomic<std::uint64_t> next_session_id_;
};

}  // namespace pvpgn::infra::session
