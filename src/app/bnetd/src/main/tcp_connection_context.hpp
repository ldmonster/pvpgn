// SPDX-License-Identifier: GPL-2.0-or-later
// main/tcp_connection_context.hpp — IConnectionContext backed by TcpSessionEgress.
//
// Minimal IConnectionContext implementation that forwards send_packet/close
// to a TcpSessionEgress. Used by BnetConnectionAdapter in the composition
// root so that ConnectionFsm can send BNCS packets over the TCP transport.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "domain/connection/connection_context.hpp"
#include "app/bnetd/tcp_session.hpp"

namespace pvpgn::app::bnetd {

class TcpConnectionContext final
    : public domain::connection::IConnectionContext {
public:
    TcpConnectionContext(std::shared_ptr<TcpSessionEgress> egress,
                         std::string                        remote_addr,
                         std::uint32_t                      session_id) noexcept
        : egress_(std::move(egress))
        , remote_addr_(std::move(remote_addr))
        , session_id_(session_id) {}

    [[nodiscard]] core::Status<> send_packet(
        std::uint8_t packet_id,
        std::span<const std::byte> payload) override {
        // Build a minimal 4-byte BNCS header + payload and send.
        // Header: 0xFF, packet_id, length (LE uint16)
        //
        // R169.e: avoid the reserve+push_back+insert pattern that
        // gcc 15 mis-diagnoses as `-Werror=free-nonheap-object` on
        // -O2. Pre-size the vector and write through indices.
        const std::size_t total = 4u + payload.size();
        const std::uint16_t total_le = static_cast<std::uint16_t>(total);
        std::vector<std::byte> buf(total);
        buf[0] = std::byte{0xFF};
        buf[1] = std::byte{packet_id};
        buf[2] = std::byte{static_cast<std::uint8_t>(total_le & 0xFFu)};
        buf[3] = std::byte{static_cast<std::uint8_t>((total_le >> 8) & 0xFFu)};
        if (!payload.empty()) {
            std::copy(payload.begin(), payload.end(), buf.begin() + 4);
        }
        egress_->send(std::move(buf));
        return core::ok();
    }

    void close() override {
        egress_->close();
    }

    [[nodiscard]] std::string get_remote_address() const override {
        return remote_addr_;
    }

    [[nodiscard]] std::uint32_t get_session_id() const override {
        return session_id_;
    }

    void on_game_created(std::uint32_t /*game_id*/,
                         const domain::connection::GameInfo& /*info*/) override {
        // TODO(Phase3): wire into game registry
    }

    void on_game_joined(std::uint32_t /*game_id*/,
                        const domain::connection::GameInfo& /*info*/) override {
        // TODO(Phase3): wire into game registry
    }

    void on_game_left(std::uint32_t /*game_id*/) override {
        // TODO(Phase3): wire into game registry
    }

private:
    std::shared_ptr<TcpSessionEgress> egress_;
    std::string                        remote_addr_;
    std::uint32_t                      session_id_;
};

} // namespace pvpgn::app::bnetd
