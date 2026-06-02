// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file logging_connection_context.hpp
/// Concrete `IConnectionContext` implementation that logs all callbacks to
/// `std::cout` with a `[ConnectionFsm]` prefix.
///
/// This is a simple, header-only implementation used in:
///   - The composition root (`main.cpp`) as the domain-level context for
///     `BnetConnectionAdapter` in standalone / development mode.
///   - Unit tests that need a concrete `IConnectionContext` without mocking.
///
/// It also wraps an underlying `IConnectionContext` for I/O (send_packet /
/// close / get_remote_address / get_session_id), so it can be layered on top
/// of any transport adapter.
///
/// ## C++20 conventions
///   - Header-only (all inline) — no `.cpp` file needed
///   - No exceptions; errors returned as `core::Status<>`
///   - `[[nodiscard]]` on all status-returning methods

#include <cstdint>
#include <iostream>
#include <span>
#include <string>

#include "core/result.hpp"
#include "domain/connection/connection_context.hpp"

#include "domain/connection/ports.hpp"

namespace pvpgn::app::bnetd {

// ---------------------------------------------------------------------------
// LoggingConnectionContext
// ---------------------------------------------------------------------------

/// `IConnectionContext` implementation that logs all domain callbacks.
///
/// Wraps an underlying `IConnectionContext` for I/O operations.
/// The wrapped context MUST outlive this object.
class LoggingConnectionContext final
    : public domain::connection::IConnectionContext {
public:
    /// Construct with an underlying I/O context.
    ///
    /// @param inner  The underlying context for send_packet / close /
    ///               get_remote_address / get_session_id. Non-owning ref;
    ///               MUST outlive this object.
    explicit LoggingConnectionContext(
        domain::connection::IConnectionContext& inner) noexcept
        : inner_(inner) {}

    // Non-copyable, non-movable (holds a reference to inner_).
    LoggingConnectionContext(const LoggingConnectionContext&)            = delete;
    LoggingConnectionContext& operator=(const LoggingConnectionContext&) = delete;
    LoggingConnectionContext(LoggingConnectionContext&&)                 = delete;
    LoggingConnectionContext& operator=(LoggingConnectionContext&&)      = delete;

    ~LoggingConnectionContext() override = default;

    // -----------------------------------------------------------------------
    // IConnectionContext — I/O forwarded to inner_
    // -----------------------------------------------------------------------

    [[nodiscard]] core::Status<> send_packet(
        std::uint8_t packet_id,
        std::span<const std::byte> payload) override {
        return inner_.send_packet(packet_id, payload);
    }

    void close() override {
        std::cout << "[ConnectionFsm] session " << inner_.get_session_id()
                  << " close()\n";
        inner_.close();
    }

    [[nodiscard]] std::string get_remote_address() const override {
        return inner_.get_remote_address();
    }

    [[nodiscard]] std::uint32_t get_session_id() const override {
        return inner_.get_session_id();
    }

    // -----------------------------------------------------------------------
    // IConnectionContext — game lifecycle callbacks (logged)
    // -----------------------------------------------------------------------

    void on_game_created(std::uint32_t game_id,
                         const domain::connection::GameInfo& info) override {
        std::cout << "[ConnectionFsm] session " << inner_.get_session_id()
                  << " on_game_created: game_id=" << game_id
                  << " name=\"" << info.game_name << "\"\n";
    }

    void on_game_joined(std::uint32_t game_id,
                        const domain::connection::GameInfo& info) override {
        std::cout << "[ConnectionFsm] session " << inner_.get_session_id()
                  << " on_game_joined: game_id=" << game_id
                  << " name=\"" << info.game_name << "\"\n";
    }

    void on_game_left(std::uint32_t game_id) override {
        std::cout << "[ConnectionFsm] session " << inner_.get_session_id()
                  << " on_game_left: game_id=" << game_id << "\n";
    }

private:
    domain::connection::IConnectionContext& inner_;
};

}  // namespace pvpgn::app::bnetd
