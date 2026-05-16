#include "protocol/d2cs/fsm.hpp"
#include <cstring>
#include <algorithm>

namespace pvpgn::protocol::d2cs {

D2CSSessionFsm::D2CSSessionFsm(D2CSFsmCallbacks callbacks)
    : callbacks_(std::move(callbacks))
{
}

core::Result<size_t, core::Error> D2CSSessionFsm::feed(const uint8_t* data, size_t len) {
    if (!data || len == 0) {
        return core::Result<size_t, core::Error>(0);
    }
    
    buffer_.insert(buffer_.end(), data, data + len);
    size_t consumed = 0;
    
    while (buffer_.size() >= 3) {
        // Parse header: length (2 bytes) + type (1 byte)
        uint16_t packet_len = (buffer_[0] | (buffer_[1] << 8));
        uint8_t packet_type = buffer_[2];
        
        if (packet_len < 3 || packet_len > 65535) {
            return core::fail(
                core::make_error(core::StatusCode::InvalidArgument, "Invalid packet length")
            );
        }
        
        if (buffer_.size() < packet_len) {
            break;  // Wait for more data
        }
        
        // Extract payload (skip header)
        const uint8_t* payload = buffer_.data() + 3;
        size_t payload_len = packet_len - 3;
        
        auto dispatch_result = dispatch(static_cast<D2CSPacketType>(packet_type), payload, payload_len);
        if (!dispatch_result) {
            return core::fail(std::move(dispatch_result).error());
        }
        
        // Remove processed packet from buffer
        buffer_.erase(buffer_.begin(), buffer_.begin() + packet_len);
        consumed += packet_len;
    }
    
    return core::Result<size_t, core::Error>(consumed);
}

core::Result<void, core::Error> D2CSSessionFsm::dispatch(D2CSPacketType type, const uint8_t* payload, size_t len) {
    switch (type) {
        case D2CSPacketType::LOGINREQ:
            return handle_login(payload, len);
        case D2CSPacketType::CHARLOGINREQ:
            return handle_char_login(payload, len);
        case D2CSPacketType::CREATEGAMEREQ:
            return handle_create_game(payload, len);
        case D2CSPacketType::JOINGAMEREQ:
            return handle_join_game(payload, len);
        default:
            return core::fail(
                core::make_error(core::StatusCode::InvalidArgument, "Unknown packet type")
            );
    }
}

core::Result<void, core::Error> D2CSSessionFsm::handle_login(const uint8_t* payload, size_t len) {
    if (len < 8) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument, "Login request too short")
        );
    }
    
    D2CSLoginRequest req;
    req.account_id = (payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24));
    req.session_key = (payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24));
    
    if (callbacks_.on_login) {
        auto result = callbacks_.on_login(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
        state_ = D2CSSessionState::authenticating;
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_char_login(const uint8_t* payload, size_t len) {
    if (len < 4) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument, "Char login request too short")
        );
    }
    
    D2CSCharLoginRequest req;
    req.client_token = (payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24));
    
    if (callbacks_.on_char_login) {
        auto result = callbacks_.on_char_login(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
        state_ = D2CSSessionState::authenticated;
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_create_game(const uint8_t* payload, size_t len) {
    if (len < 2) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument, "Create game request too short")
        );
    }
    
    D2CSCreateGameRequest req;
    req.difficulty = payload[0];
    req.is_expansion = (payload[1] != 0);
    
    if (callbacks_.on_create_game) {
        auto result = callbacks_.on_create_game(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
        state_ = D2CSSessionState::in_game;
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2CSSessionFsm::handle_join_game(const uint8_t* payload, size_t len) {
    if (len < 1) {
        return core::fail(
            core::make_error(core::StatusCode::InvalidArgument, "Join game request too short")
        );
    }
    
    D2CSJoinGameRequest req;
    
    if (callbacks_.on_join_game) {
        auto result = callbacks_.on_join_game(req);
        if (!result) {
            return core::fail(std::move(result).error());
        }
        state_ = D2CSSessionState::in_game;
    }
    
    return core::Result<void, core::Error>();
}

std::vector<uint8_t> D2CSSessionFsm::make_login_reply(uint8_t result_code) {
    std::vector<uint8_t> reply;
    reply.push_back(0x04);  // length low
    reply.push_back(0x00);  // length high
    reply.push_back(static_cast<uint8_t>(D2CSPacketType::LOGINREPLY));
    reply.push_back(result_code);
    return reply;
}

std::vector<uint8_t> D2CSSessionFsm::make_char_login_reply(uint8_t result_code) {
    std::vector<uint8_t> reply;
    reply.push_back(0x04);  // length low
    reply.push_back(0x00);  // length high
    reply.push_back(static_cast<uint8_t>(D2CSPacketType::CHARLOGINREPLY));
    reply.push_back(result_code);
    return reply;
}

std::vector<uint8_t> D2CSSessionFsm::make_create_game_reply(uint8_t result_code, uint32_t token) {
    std::vector<uint8_t> reply;
    reply.push_back(0x08);  // length low
    reply.push_back(0x00);  // length high
    reply.push_back(static_cast<uint8_t>(D2CSPacketType::CREATEGAMEREPLY));
    reply.push_back(result_code);
    reply.push_back(token & 0xFF);
    reply.push_back((token >> 8) & 0xFF);
    reply.push_back((token >> 16) & 0xFF);
    reply.push_back((token >> 24) & 0xFF);
    return reply;
}

std::vector<uint8_t> D2CSSessionFsm::make_join_game_reply(uint8_t result_code, std::string_view gs_addr, uint16_t gs_port) {
    std::vector<uint8_t> reply;
    size_t addr_len = gs_addr.length();
    uint16_t total_len = 4 + addr_len + 2;
    
    reply.push_back(total_len & 0xFF);
    reply.push_back((total_len >> 8) & 0xFF);
    reply.push_back(static_cast<uint8_t>(D2CSPacketType::JOINGAMEREPLY));
    reply.push_back(result_code);
    
    reply.insert(reply.end(), gs_addr.begin(), gs_addr.end());
    
    reply.push_back(gs_port & 0xFF);
    reply.push_back((gs_port >> 8) & 0xFF);
    
    return reply;
}

std::vector<uint8_t> D2CSSessionFsm::make_char_list_reply(const std::vector<std::string>& char_names) {
    std::vector<uint8_t> reply;
    
    // Calculate total length
    size_t payload_len = 1;  // char count
    for (const auto& name : char_names) {
        payload_len += name.length() + 1;  // name + null terminator
    }
    uint16_t total_len = 3 + payload_len;
    
    reply.push_back(total_len & 0xFF);
    reply.push_back((total_len >> 8) & 0xFF);
    reply.push_back(static_cast<uint8_t>(D2CSPacketType::CHARLISTREPLY));
    reply.push_back(static_cast<uint8_t>(char_names.size()));
    
    for (const auto& name : char_names) {
        reply.insert(reply.end(), name.begin(), name.end());
        reply.push_back(0x00);  // null terminator
    }
    
    return reply;
}

} // namespace pvpgn::protocol::d2cs
