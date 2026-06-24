// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file file_session_factory.hpp
/// SessionFactory for BNFTP file-serving protocol.
/// Creates BnftpFsm for each accepted connection.

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include <boost/asio/ip/tcp.hpp>

#include "domain/identity/ports.hpp"
#include "core/bytes.hpp"
#include "domain/shared/ids.hpp"
#include "infra/net/tcp_session.hpp"
#include "protocol/file/bnftp_fsm.hpp"

namespace pvpgn::infra::session {

/// Factory that creates a file-serving session.
/// Each connection is handled by BnftpFsm.
class FileSessionFactory {
public:
    /// Create a factory with the given dependencies.
    FileSessionFactory(
        std::shared_ptr<domain::identity::ISessionRegistry> registry,
        std::string files_dir)
        : registry_(registry), files_dir_(files_dir) {}

    /// Called by TcpAcceptor for each accepted connection.
    /// Instantiates BnftpFsm to handle the session.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp_session) {
        if (!tcp_session) return;

        // Generate a new SessionId
        domain::SessionId session_id{next_session_id_.fetch_add(1)};

        // TODO: Create ISessionContext wrapper for tcp_session
        // For now, we instantiate the FSM but don't wire it
        // auto fsm = std::make_shared<protocol::file::BnftpFsm>(
        //     session_context, files_dir_);

        // Set up TCP session callbacks
        tcp_session->set_on_bytes([session_id](core::ByteView bytes) {
            // TODO: Feed bytes to BnftpFsm::on_bytes
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
    std::string files_dir_;
    static std::atomic<std::uint64_t> next_session_id_;
};

}  // namespace pvpgn::infra::session
