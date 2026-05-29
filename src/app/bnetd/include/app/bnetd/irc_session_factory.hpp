// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file irc_session_factory.hpp
/// `IrcSessionFactory` — callable that creates `IrcTcpSession` instances.
///
/// Design
/// ------
/// `TcpListener` requires a `SessionFactory` callback with signature:
///   `void(std::shared_ptr<infra::net::TcpSession>)`
///
/// `IrcSessionFactory` is a copyable callable that captures the server
/// name and produces a fully-wired `IrcTcpSession` for each accepted
/// connection.
///
/// Usage
/// -----
/// ```cpp
/// IrcSessionFactory factory{config.server_name};
/// TcpListener irc_listener{rt, factory};
/// irc_listener.start(config.listen_address, config.irc_port);
/// ```
///
/// Thread-safety
/// -------------
/// `operator()` is called from an Asio worker thread (inside
/// `TcpAcceptor`'s accept handler). The factory itself is stateless
/// after construction (server_name_ is read-only), so concurrent calls
/// are safe.

#include <memory>
#include <string>

#include "infra/net/tcp_session.hpp"

#include "app/bnetd/irc_tcp_session.hpp"

namespace pvpgn::app::bnetd {

/// Creates `IrcTcpSession` instances for each accepted TCP connection.
class IrcSessionFactory {
public:
    /// @param server_name  Hostname announced in IRC numeric reply prefixes
    ///                     (e.g. ":pvpgn.server 001 nick :Welcome to pvpgn.server").
    ///                     Stored by value; the factory is copyable.
    explicit IrcSessionFactory(std::string server_name) noexcept
        : server_name_(std::move(server_name)) {}

    /// Called by `TcpListener` for each accepted connection.
    /// Creates an `IrcTcpSession`, starts it, and returns it.
    /// The session keeps itself alive via `shared_from_this` until closed.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp) const {
        if (!tcp) return;
        auto session = std::make_shared<IrcTcpSession>(std::move(tcp),
                                                        server_name_);
        session->start();
    }

    /// Expose the configured server name (for testing / logging).
    [[nodiscard]] const std::string& server_name() const noexcept {
        return server_name_;
    }

private:
    std::string server_name_;
};

}  // namespace pvpgn::app::bnetd
