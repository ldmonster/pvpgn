// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/wol/wol_session_factory.hpp"

#include <cstring>
#include <span>

namespace pvpgn::integration::wol {

WolSession::WolSession(std::string session_id)
    : session_id_(std::move(session_id)) {}

core::Result<std::vector<uint8_t>, core::Error> WolSession::feed(
    std::span<const uint8_t> data) {
    
    // Append incoming data to buffer
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    
    std::vector<uint8_t> replies;
    
    // Process complete packets from buffer
    while (buffer_.size() >= 2) {
        // WOL packet format: [type:1][length:1][payload:length-2]
        uint8_t packet_type = buffer_[0];
        uint8_t packet_len = buffer_[1];
        
        if (packet_len < 2 || packet_len > 255) {
            return core::fail(core::Error(
                core::StatusCode::InvalidArgument,
                "Invalid WOL packet length"
            ));
        }
        
        if (buffer_.size() < packet_len) {
            // Incomplete packet, wait for more data
            break;
        }
        
        // Extract packet payload
        std::span<const uint8_t> payload(buffer_.data() + 2, packet_len - 2);
        
        // Handle packet based on type
        core::Result<std::vector<uint8_t>, core::Error> result;
        
        switch (static_cast<WolPacketType>(packet_type)) {
            case WolPacketType::LOGIN_REQUEST:
                result = handle_login(payload);
                break;
            case WolPacketType::PING:
                result = handle_ping();
                break;
            default:
                // Unknown packet type, skip it
                result = core::Result<std::vector<uint8_t>, core::Error>{std::vector<uint8_t>{}};
                break;
        }
        
        if (!result) {
            return result;
        }
        
        // Append reply to output
        auto reply = std::move(result).value();
        replies.insert(replies.end(), reply.begin(), reply.end());
        
        // Remove processed packet from buffer
        buffer_.erase(buffer_.begin(), buffer_.begin() + packet_len);
    }
    
    return core::Result<std::vector<uint8_t>, core::Error>{replies};
}

core::Result<std::vector<uint8_t>, core::Error> WolSession::handle_login(
    std::span<const uint8_t> payload) {
    
    if (payload.size() < 1) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "LOGIN_REQUEST payload too short"
        ));
    }
    
    // Transition to authenticated state
    state_ = State::authenticated;
    
    // Create LOGIN_REPLY packet
    std::vector<uint8_t> reply_payload;
    reply_payload.push_back(0x00);  // Success code
    
    return core::Result<std::vector<uint8_t>, core::Error>{
        make_reply(WolPacketType::LOGIN_REPLY, reply_payload)
    };
}

core::Result<std::vector<uint8_t>, core::Error> WolSession::handle_ping() {
    // Create PONG packet
    return core::Result<std::vector<uint8_t>, core::Error>{
        make_reply(WolPacketType::PONG, {})
    };
}

std::vector<uint8_t> WolSession::make_reply(WolPacketType type,
                                             std::span<const uint8_t> payload) {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(type));
    packet.push_back(static_cast<uint8_t>(payload.size() + 2));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::unique_ptr<WolSession> WolSessionFactory::create(std::string session_id) {
    return std::make_unique<WolSession>(std::move(session_id));
}

}  // namespace pvpgn::integration::wol
