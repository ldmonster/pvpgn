// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file file_session_factory.hpp
/// `FileSessionFactory` — callable that creates `BnftpTcpSession` instances.
///
/// Design
/// ------
/// `TcpListener` requires a `SessionFactory` callback with signature:
///   `void(std::shared_ptr<infra::net::TcpSession>)`
///
/// `FileSessionFactory` is a copyable callable that captures the data
/// directory and produces a fully-wired `BnftpTcpSession` for each
/// accepted connection.
///
/// Usage
/// -----
/// ```cpp
/// FileSessionFactory factory{config.data_dir};
/// TcpListener bnftp_listener{rt, factory};
/// bnftp_listener.start(config.listen_address, config.bnftp_port);
/// ```
///
/// Thread-safety
/// -------------
/// `operator()` is called from an Asio worker thread (inside
/// `TcpAcceptor`'s accept handler). The factory itself is stateless
/// after construction (data_dir_ is read-only), so concurrent calls
/// are safe.

#include <filesystem>
#include <memory>

#include "infra/net/tcp_session.hpp"

#include "app/bnetd/bnftp_tcp_session.hpp"

namespace pvpgn::app::bnetd {

/// Creates `BnftpTcpSession` instances for each accepted TCP connection.
class FileSessionFactory {
public:
    /// @param data_dir  Root directory from which BNFTP files are served.
    ///                  Stored by value; the factory is copyable.
    explicit FileSessionFactory(std::filesystem::path data_dir) noexcept
        : data_dir_(std::move(data_dir)) {}

    /// Called by `TcpListener` for each accepted connection.
    /// Creates a `BnftpTcpSession`, starts it, and returns it.
    /// The session keeps itself alive via `shared_from_this` until closed.
    void operator()(std::shared_ptr<infra::net::TcpSession> tcp) const {
        if (!tcp) return;
        auto session = std::make_shared<BnftpTcpSession>(std::move(tcp),
                                                          data_dir_);
        session->start();
    }

    /// Expose the configured data directory (for testing / logging).
    [[nodiscard]] const std::filesystem::path& data_dir() const noexcept {
        return data_dir_;
    }

private:
    std::filesystem::path data_dir_;
};

}  // namespace pvpgn::app::bnetd
