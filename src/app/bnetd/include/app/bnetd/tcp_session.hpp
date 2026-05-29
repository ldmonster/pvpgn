// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file tcp_session.hpp
/// `IConnectionEgress` adapters that bridge `infra::net::TcpSession` to the
/// FSM session-context interfaces used by the v3 bnetd composition root.
///
/// Architecture
/// ------------
/// The existing v3 infrastructure already provides:
///
///   infra::net::TcpSession          — Asio async read/write engine
///   protocol::bnet::BnetSessionContextImpl — encodes ServerMessage → bytes
///                                            via Writer + IConnectionEgress
///   protocol::file::IFileSessionContext    — raw byte send + close
///   protocol::wol::IWolSessionContext      — line/byte send + close
///
/// This file provides one concrete `IConnectionEgress` implementation
/// (`TcpSessionEgress`) that forwards `send(bytes)` / `close()` to an
/// `infra::net::TcpSession`. It is the glue between the codec layer and
/// the Asio transport.
///
/// Usage pattern (BNet)
/// --------------------
///   auto tcp   = infra::net::TcpSession::create(std::move(socket));
///   auto egress = std::make_shared<TcpSessionEgress>(tcp);
///   auto ctx    = std::make_shared<protocol::bnet::BnetSessionContextImpl>(
///                     session_id, egress);
///   auto fsm    = std::make_shared<protocol::bnet::BnetFsm>(ctx, use_cases,
///                                                            session_id);
///   tcp->set_on_bytes([fsm](core::ByteView bv) {
///       // decode + feed to FSM (see BnetSessionFactory for full pattern)
///   });
///   tcp->set_on_close([ctx](auto) { ctx->close(); });
///   tcp->start();
///
/// Usage pattern (BNFTP)
/// ---------------------
///   auto tcp    = infra::net::TcpSession::create(std::move(socket));
///   auto egress = std::make_shared<TcpSessionEgress>(tcp);
///   auto ctx    = std::make_shared<BnftpEgressContext>(egress);
///   auto fsm    = std::make_shared<protocol::file::BnftpFsm>(ctx, files_dir);
///   tcp->set_on_bytes([fsm](core::ByteView bv) {
///       auto sp = std::span<const std::byte>(bv.data(), bv.size());
///       fsm->on_bytes(sp);
///   });
///   tcp->set_on_close([fsm](auto) { fsm->on_close(); });
///   tcp->start();
///
/// Usage pattern (WOL)
/// -------------------
///   Similar to BNFTP but using WolEgressContext.

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/connection_handler.hpp"
#include "core/result.hpp"
#include "infra/net/tcp_session.hpp"
#include "protocol/file/file_session_context.hpp"
#include "protocol/wol/wol_session_context.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// TcpSessionEgress
// ---------------------------------------------------------------------------

/// Implements `IConnectionEgress` by forwarding to `infra::net::TcpSession`.
///
/// This is the canonical egress adapter for all protocol FSMs in the v3
/// bnetd composition root. It is shared between:
///   * `BnetSessionContextImpl` (BNet protocol)
///   * `BnftpEgressContext`     (BNFTP file transfer)
///   * `WolEgressContext`       (WOL chat)
class TcpSessionEgress final : public application::ports::IConnectionEgress {
public:
    explicit TcpSessionEgress(
        std::shared_ptr<infra::net::TcpSession> tcp) noexcept
        : tcp_(std::move(tcp)) {}

    void send(std::vector<std::byte> bytes) override {
        if (tcp_) tcp_->send(std::move(bytes));
    }

    void close() override {
        if (tcp_) tcp_->close();
    }

private:
    std::shared_ptr<infra::net::TcpSession> tcp_;
};

// ---------------------------------------------------------------------------
// BnftpEgressContext
// ---------------------------------------------------------------------------

/// Implements `protocol::file::IFileSessionContext` on top of
/// `TcpSessionEgress`. Used by `BnftpFsm`.
class BnftpEgressContext final : public protocol::file::IFileSessionContext {
public:
    explicit BnftpEgressContext(
        std::shared_ptr<TcpSessionEgress> egress) noexcept
        : egress_(std::move(egress)) {}

    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        if (!egress_) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "null egress"});
        }
        egress_->send(std::vector<std::byte>(bytes.begin(), bytes.end()));
        return core::ok();
    }

    void close() override {
        if (egress_) egress_->close();
    }

private:
    std::shared_ptr<TcpSessionEgress> egress_;
};

// ---------------------------------------------------------------------------
// WolEgressContext
// ---------------------------------------------------------------------------

/// Implements `protocol::wol::IWolSessionContext` on top of
/// `TcpSessionEgress`. Used by `WolFsm`.
///
/// `send_line(line)` appends `\r\n` if not already present.
class WolEgressContext final : public protocol::wol::IWolSessionContext {
public:
    WolEgressContext(std::shared_ptr<TcpSessionEgress> egress,
                     std::string                        server_name) noexcept
        : egress_(std::move(egress)), server_name_(std::move(server_name)) {}

    core::Status<> send_line(std::string_view line) override {
        if (!egress_) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "null egress"});
        }
        std::vector<std::byte> buf;
        buf.reserve(line.size() + 2);
        for (char c : line) buf.push_back(static_cast<std::byte>(c));
        // Append \r\n if not already present
        const bool has_crlf = line.size() >= 2 &&
                               line[line.size() - 2] == '\r' &&
                               line[line.size() - 1] == '\n';
        if (!has_crlf) {
            buf.push_back(static_cast<std::byte>('\r'));
            buf.push_back(static_cast<std::byte>('\n'));
        }
        egress_->send(std::move(buf));
        return core::ok();
    }

    core::Status<> send_bytes(std::span<const std::byte> bytes) override {
        if (!egress_) {
            return core::fail(core::Error{core::StatusCode::Internal,
                                          "null egress"});
        }
        egress_->send(std::vector<std::byte>(bytes.begin(), bytes.end()));
        return core::ok();
    }

    void close() override {
        if (egress_) egress_->close();
    }

    std::string_view server_name() const noexcept override {
        return server_name_;
    }

private:
    std::shared_ptr<TcpSessionEgress> egress_;
    std::string                        server_name_;
};

}  // namespace pvpgn::app::bnetd
