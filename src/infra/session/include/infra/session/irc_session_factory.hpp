// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file irc_session_factory.hpp
/// SessionFactory for IRC protocol (Phase 6b).
/// Creates IrcBridgeFsm for each accepted connection.

#include <atomic>
#include <cstdint>
#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "domain/identity/ports.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "infra/net/tcp_session.hpp"
#include "protocol/irc/bridge_fsm.hpp"

namespace pvpgn::infra::session {

/// Factory that creates an IRC protocol session.
/// Each connection is handled by IrcBridgeFsm.
class IrcSessionFactory {
public:
    /// Use-case context for IRC bridge.
    using UseCaseContext = pvpgn::protocol::irc::IrcBridgeFsm::UseCaseContext;

    /// Create a factory with the given dependencies.
    IrcSessionFactory(
        std::shared_ptr<application::ports::ISessionRegistry> registry,
        UseCaseContext use_cases)
        : registry_(registry), use_cases_(use_cases) {}

    /// Called by TcpAcceptor for each accepted connection.
    /// Instantiates IrcBridgeFsm to handle the session.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp_session) {
        if (!tcp_session) return;

        // Generate a new SessionId
        domain::SessionId session_id{next_session_id_.fetch_add(1)};

        // TODO: Create ISessionContext wrapper for tcp_session
        // For now, we instantiate the FSM but don't wire it
        // auto fsm = std::make_shared<protocol::irc::IrcBridgeFsm>(
        //     session_context, use_cases_);

        // Set up TCP session callbacks
        tcp_session->set_on_bytes([session_id](core::ByteView bytes) {
            // TODO: Feed bytes to IrcBridgeFsm::handle
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
    std::weak_ptr<application::ports::ISessionRegistry> registry_;
    UseCaseContext use_cases_;
    static std::atomic<std::uint64_t> next_session_id_;
};

}  // namespace pvpgn::infra::session
