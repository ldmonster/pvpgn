// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file telnet_session_factory.hpp
/// SessionFactory for Telnet admin console.
/// Creates TelnetAdminFsm for each accepted connection.

#include <atomic>
#include <cstdint>
#include <memory>

#include <boost/asio/ip/tcp.hpp>

#include "domain/identity/ports.hpp"
#include "domain/chat/ports/command_registry.hpp"
#include "domain/moderation/ports.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "infra/net/tcp_session.hpp"
#include "protocol/telnet/admin_fsm.hpp"

namespace pvpgn::infra::session {

/// Factory that creates a Telnet admin console session.
/// Each connection is handled by TelnetAdminFsm.
class TelnetSessionFactory {
public:
    /// Create a factory with the given dependencies.
    TelnetSessionFactory(
        std::shared_ptr<domain::identity::ISessionRegistry> registry,
        std::shared_ptr<application::ports::ICommandRegistry> commands,
        std::shared_ptr<domain::moderation::IPermissionChecker> permissions)
        : registry_(registry), commands_(commands), permissions_(permissions) {}

    /// Called by TcpAcceptor for each accepted connection.
    /// Instantiates TelnetAdminFsm to handle the session.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp_session) {
        if (!tcp_session) return;

        // Generate a new SessionId
        domain::SessionId session_id{next_session_id_.fetch_add(1)};

        // TODO: Create ISessionContext wrapper for tcp_session
        // For now, we instantiate the FSM but don't wire it
        // auto fsm = std::make_shared<protocol::telnet::TelnetAdminFsm>(
        //     session_context, commands_, permissions_);

        // Set up TCP session callbacks
        tcp_session->set_on_bytes([session_id](core::ByteView bytes) {
            // TODO: Feed bytes to TelnetAdminFsm::on_line
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
    std::shared_ptr<application::ports::ICommandRegistry> commands_;
    std::shared_ptr<domain::moderation::IPermissionChecker> permissions_;
    static std::atomic<std::uint64_t> next_session_id_;
};

}  // namespace pvpgn::infra::session
