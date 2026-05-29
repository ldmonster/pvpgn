// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file bnftp_tcp_session.hpp
/// `BnftpTcpSession` — wires a `infra::net::TcpSession` to a `BnftpFsm`.
///
/// Architecture
/// ------------
/// The BNFTP file-transfer protocol is a simple request/response protocol:
///   1. Client sends CLIENT_FILE_REQ (first byte 0x01)
///   2. Server replies with SERVER_FILE_REPLY header + raw file bytes
///   3. Connection closes after transfer
///
/// `BnftpTcpSession` owns the full object graph for one BNFTP connection:
///
///   TcpSession (Asio transport)
///       └─► TcpSessionEgress (IConnectionEgress adapter)
///               └─► BnftpEgressContext (IFileSessionContext adapter)
///                       └─► BnftpFsm (protocol state machine)
///
/// The session is created by `FileSessionFactory` and is self-contained:
/// once `start()` is called the session drives itself until the FSM
/// transitions to `Done` and the underlying socket is closed.
///
/// Thread-safety
/// -------------
/// All callbacks are invoked from Asio worker threads. The underlying
/// `TcpSession` serialises writes via its internal strand; `BnftpFsm`
/// is single-threaded and must only be called from the Asio thread that
/// owns the session (guaranteed by `TcpSession`'s sequential callbacks).

#include <filesystem>
#include <memory>
#include <string>

#include "infra/net/tcp_session.hpp"
#include "protocol/file/bnftp_fsm.hpp"
#include "protocol/file/file_session_context.hpp"

#include "app/bnetd/tcp_session.hpp"  // TcpSessionEgress, BnftpEgressContext

namespace pvpgn::app::bnetd {

/// Owns one BNFTP connection: TcpSession + egress adapters + BnftpFsm.
///
/// Lifetime: `shared_ptr` — the Asio callbacks keep the session alive
/// until the socket closes and all pending writes complete.
class BnftpTcpSession
    : public std::enable_shared_from_this<BnftpTcpSession> {
public:
    /// Create a session from an accepted socket and a data directory.
    ///
    /// @param tcp       Accepted TCP session (from `TcpSession::create`).
    /// @param data_dir  Directory from which BNFTP files are served.
    ///                  Passed verbatim to `BnftpFsm` as `files_dir`.
    BnftpTcpSession(std::shared_ptr<infra::net::TcpSession> tcp,
                    std::filesystem::path                    data_dir)
        : tcp_(std::move(tcp))
        , data_dir_(std::move(data_dir))
    {
        // Build the egress chain: TcpSession → TcpSessionEgress → BnftpEgressContext
        auto egress = std::make_shared<TcpSessionEgress>(tcp_);
        ctx_        = std::make_shared<BnftpEgressContext>(std::move(egress));
        fsm_        = std::make_shared<protocol::file::BnftpFsm>(
                          ctx_, data_dir_.string());
    }

    BnftpTcpSession(const BnftpTcpSession&)            = delete;
    BnftpTcpSession& operator=(const BnftpTcpSession&) = delete;
    BnftpTcpSession(BnftpTcpSession&&)                 = delete;
    BnftpTcpSession& operator=(BnftpTcpSession&&)      = delete;

    ~BnftpTcpSession() = default;

    /// Wire callbacks and begin reading from the socket.
    ///
    /// Must be called exactly once after construction. After `start()` the
    /// session is self-sustaining: Asio callbacks hold a `shared_ptr` to
    /// `this` until the socket closes.
    void start() {
        auto self = shared_from_this();

        tcp_->set_on_bytes([self](core::ByteView bv) {
            auto sp = std::span<const std::byte>(bv.data(), bv.size());
            (void)self->fsm_->on_bytes(sp);
        });

        tcp_->set_on_close([self](const boost::system::error_code&) {
            self->fsm_->on_close();
        });

        tcp_->start();
    }

    /// Expose the FSM for testing (state inspection).
    [[nodiscard]] const protocol::file::BnftpFsm& fsm() const noexcept {
        return *fsm_;
    }

    /// Expose the underlying IFileSessionContext for testing.
    [[nodiscard]] std::shared_ptr<protocol::file::IFileSessionContext>
    context() const noexcept {
        return ctx_;
    }

private:
    std::shared_ptr<infra::net::TcpSession>              tcp_;
    std::filesystem::path                                data_dir_;
    std::shared_ptr<protocol::file::IFileSessionContext> ctx_;
    std::shared_ptr<protocol::file::BnftpFsm>            fsm_;
};

}  // namespace pvpgn::app::bnetd
