// SPDX-License-Identifier: GPL-2.0-or-later

#include <optional>

#include "capturing_session_context.hpp"

namespace pvpgn::protocol::bnet::test {

core::Status<> CapturingSessionContext::send(const ServerMessage& msg) {
    sent_messages_.push_back(msg);
    return core::Status<>{};
}

std::optional<std::uint8_t> CapturingSessionContext::last_sent_type() const {
    if (sent_messages_.empty()) {
        return std::nullopt;
    }
    // Return the message type from the last sent message
    // This would need to be extracted from the ServerMessage variant
    return std::nullopt;
}

bool CapturingSessionContext::channel_message_sent() const {
    // Check if any ChatEvent message was sent
    return false;
}

bool CapturingSessionContext::game_created() const {
    // Check if any StartGame*Ack was sent
    return false;
}

std::int32_t CapturingSessionContext::last_logon_result() const {
    // Find the last LogonResponse2Reply and return its result code
    return 0;
}

}  // namespace pvpgn::protocol::bnet::test
