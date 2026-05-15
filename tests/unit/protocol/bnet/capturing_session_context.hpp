// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file capturing_session_context.hpp
/// Test helper for capturing and verifying session messages.

#include <cstdint>
#include <memory>
#include <vector>

#include "protocol/bnet/messages.hpp"
#include "protocol/bnet/session_context.hpp"

namespace pvpgn::protocol::bnet::test {

/// Mock ISessionContext that captures all sent messages for test inspection.
class CapturingSessionContext : public ISessionContext {
public:
    CapturingSessionContext() = default;
    ~CapturingSessionContext() = default;

    /// Send a message (captured in sent_messages_).
    void send(const ServerMessage& msg) override;

    /// Mark session as closed.
    void close() override { closed_ = true; }

    /// Return a constant session ID for testing.
    domain::SessionId session_id() const override {
        return domain::SessionId{1};
    }

    // Query helpers for test assertions

    /// Get the most recently sent message type.
    /// Returns std::nullopt if no messages have been sent.
    std::optional<std::uint8_t> last_sent_type() const;

    /// Check if a ChatEvent message was sent with message type.
    bool channel_message_sent() const;

    /// Check if a game was created (StartGame*Ack sent).
    bool game_created() const;

    /// Check if session was closed.
    bool closed() const { return closed_; }

    /// Get logon result code from the last LogonResponse2Reply sent.
    /// Returns 0 if no logon reply sent yet.
    std::int32_t last_logon_result() const;

    /// Get all sent messages for detailed inspection.
    const std::vector<ServerMessage>& all_sent() const {
        return sent_messages_;
    }

    /// Clear the sent messages list (for test setup).
    void clear_sent() { sent_messages_.clear(); }

private:
    std::vector<ServerMessage> sent_messages_;
    bool closed_{false};
};

}  // namespace pvpgn::protocol::bnet::test
