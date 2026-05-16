#include "protocol/d2dbs/fsm.hpp"
#include <cstring>
#include <algorithm>

namespace pvpgn::protocol::d2dbs {

namespace {

// Helper to read null-terminated string from buffer
std::string read_string(std::span<const uint8_t> data, size_t& offset) {
    std::string result;
    while (offset < data.size() && data[offset] != '\0') {
        result += static_cast<char>(data[offset]);
        ++offset;
    }
    if (offset < data.size()) {
        ++offset;  // Skip null terminator
    }
    return result;
}

// Helper to write null-terminated string to buffer
void write_string(std::vector<uint8_t>& buffer, std::string_view str) {
    buffer.insert(buffer.end(), str.begin(), str.end());
    buffer.push_back('\0');
}

} // namespace

D2DBSSessionFsm::D2DBSSessionFsm(D2DBSFsmCallbacks callbacks)
    : callbacks_(std::move(callbacks)) {
}

core::Result<size_t, core::Error> D2DBSSessionFsm::feed(std::span<const uint8_t> data) {
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    
    size_t consumed = 0;
    
    while (buffer_.size() >= 2) {
        // Packet format: [type:1][size:1][payload:size-2]
        uint8_t type = buffer_[0];
        uint8_t size = buffer_[1];
        
        if (size < 2) {
            return core::fail(core::Error(
                core::StatusCode::InvalidArgument,
                "Invalid packet size"
            ));
        }
        
        if (buffer_.size() < size) {
            break;  // Wait for more data
        }
        
        std::span<const uint8_t> payload(buffer_.data() + 2, size - 2);
        
        auto dispatch_result = dispatch(static_cast<D2DBSPacketType>(type), payload);
        if (!dispatch_result.has_value()) {
            return core::fail(dispatch_result.error());
        }
        
        buffer_.erase(buffer_.begin(), buffer_.begin() + size);
        consumed += size;
    }
    
    return core::Result<size_t, core::Error>(consumed);
}

core::Result<void, core::Error> D2DBSSessionFsm::dispatch(D2DBSPacketType type, std::span<const uint8_t> payload) {
    switch (type) {
        case D2DBSPacketType::CHARLOGINREQ:
            return handle_char_login(payload);
        case D2DBSPacketType::CHARSAVEREQ:
            return handle_char_save(payload);
        case D2DBSPacketType::CHARLISTREQ:
            return handle_char_list(payload);
        case D2DBSPacketType::CHARCREATEREQ:
            return handle_char_create(payload);
        case D2DBSPacketType::CHARDELREQ:
            return handle_char_delete(payload);
        case D2DBSPacketType::KEEPALIVE:
            return core::Result<void, core::Error>();
        default:
            return core::fail(core::Error(
                core::StatusCode::InvalidArgument,
                "Unknown packet type"
            ));
    }
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_char_login(std::span<const uint8_t> payload) {
    if (payload.size() < 8) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Char login request too small"
        ));
    }
    
    size_t offset = 0;
    D2DBSCharLoginRequest req;
    
    // Read account name
    req.account_name = read_string(payload, offset);
    
    // Read character name
    req.char_name = read_string(payload, offset);
    
    // Read client token
    if (offset + 4 > payload.size()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Incomplete char login request"
        ));
    }
    std::memcpy(&req.client_token, payload.data() + offset, sizeof(uint32_t));
    
    if (callbacks_.on_char_login) {
        auto result = callbacks_.on_char_login(req);
        if (!result.has_value()) {
            return core::fail(result.error());
        }
        // TODO: Send reply with save data
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_char_save(std::span<const uint8_t> payload) {
    if (payload.size() < 4) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Char save request too small"
        ));
    }
    
    size_t offset = 0;
    D2DBSCharSaveRequest req;
    
    // Read account name
    req.account_name = read_string(payload, offset);
    
    // Read character name
    req.char_name = read_string(payload, offset);
    
    // Read save data size
    uint32_t save_size;
    if (offset + 4 > payload.size()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Incomplete char save request"
        ));
    }
    std::memcpy(&save_size, payload.data() + offset, sizeof(uint32_t));
    offset += 4;
    
    // Read save data
    if (offset + save_size > payload.size()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Incomplete save data"
        ));
    }
    req.save_data.assign(payload.data() + offset, payload.data() + offset + save_size);
    
    if (callbacks_.on_char_save) {
        auto result = callbacks_.on_char_save(req);
        if (!result.has_value()) {
            return core::fail(result.error());
        }
        // TODO: Send reply
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_char_list(std::span<const uint8_t> payload) {
    size_t offset = 0;
    D2DBSCharListRequest req;
    
    // Read account name
    req.account_name = read_string(payload, offset);
    
    if (callbacks_.on_char_list) {
        auto result = callbacks_.on_char_list(req);
        if (!result.has_value()) {
            return core::fail(result.error());
        }
        // TODO: Send reply with character list
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_char_create(std::span<const uint8_t> payload) {
    if (payload.size() < 3) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Char create request too small"
        ));
    }
    
    size_t offset = 0;
    D2DBSCharCreateRequest req;
    
    // Read account name
    req.account_name = read_string(payload, offset);
    
    // Read character name
    req.char_name = read_string(payload, offset);
    
    // Read character class
    if (offset >= payload.size()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Incomplete char create request"
        ));
    }
    req.char_class = payload[offset++];
    
    // Read flags (expansion, hardcore)
    if (offset >= payload.size()) {
        return core::fail(core::Error(
            core::StatusCode::InvalidArgument,
            "Incomplete char create request"
        ));
    }
    uint8_t flags = payload[offset++];
    req.is_expansion = (flags & 0x01) != 0;
    req.is_hardcore = (flags & 0x02) != 0;
    
    if (callbacks_.on_char_create) {
        auto result = callbacks_.on_char_create(req);
        if (!result.has_value()) {
            return core::fail(result.error());
        }
        // TODO: Send reply
    }
    
    return core::Result<void, core::Error>();
}

core::Result<void, core::Error> D2DBSSessionFsm::handle_char_delete(std::span<const uint8_t> payload) {
    size_t offset = 0;
    D2DBSCharDeleteRequest req;
    
    // Read account name
    req.account_name = read_string(payload, offset);
    
    // Read character name
    req.char_name = read_string(payload, offset);
    
    if (callbacks_.on_char_delete) {
        auto result = callbacks_.on_char_delete(req);
        if (!result.has_value()) {
            return core::fail(result.error());
        }
        // TODO: Send reply
    }
    
    return core::Result<void, core::Error>();
}

std::vector<uint8_t> D2DBSSessionFsm::make_char_login_reply(uint8_t result, std::span<const uint8_t> save_data) {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(D2DBSPacketType::CHARLOGINREPLY));
    
    // Placeholder for size
    size_t size_offset = packet.size();
    packet.push_back(0);
    
    // Result code
    packet.push_back(result);
    
    // Save data size
    uint32_t data_size = static_cast<uint32_t>(save_data.size());
    const uint8_t* data_size_ptr = reinterpret_cast<const uint8_t*>(&data_size);
    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        packet.push_back(data_size_ptr[i]);
    }
    
    // Save data
    for (uint8_t byte : save_data) {
        packet.push_back(byte);
    }
    
    // Update size
    packet[size_offset] = static_cast<uint8_t>(packet.size());
    
    return packet;
}

std::vector<uint8_t> D2DBSSessionFsm::make_char_save_reply(uint8_t result) {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(D2DBSPacketType::CHARSAVEREPLY));
    packet.push_back(3);  // Size: type(1) + size(1) + result(1)
    packet.push_back(result);
    return packet;
}

std::vector<uint8_t> D2DBSSessionFsm::make_char_list_reply(const std::vector<std::string>& chars) {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(D2DBSPacketType::CHARLISTREPLY));
    
    // Placeholder for size
    size_t size_offset = packet.size();
    packet.push_back(0);
    
    // Character count
    uint8_t count = std::min(static_cast<uint8_t>(chars.size()), uint8_t(255));
    packet.push_back(count);
    
    // Character names
    for (size_t i = 0; i < count; ++i) {
        write_string(packet, chars[i]);
    }
    
    // Update size
    packet[size_offset] = packet.size();
    
    return packet;
}

std::vector<uint8_t> D2DBSSessionFsm::make_char_create_reply(uint8_t result) {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(D2DBSPacketType::CHARCREATEREPLY));
    packet.push_back(3);  // Size: type(1) + size(1) + result(1)
    packet.push_back(result);
    return packet;
}

std::vector<uint8_t> D2DBSSessionFsm::make_char_delete_reply(uint8_t result) {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(D2DBSPacketType::CHARDELREPLY));
    packet.push_back(3);  // Size: type(1) + size(1) + result(1)
    packet.push_back(result);
    return packet;
}

std::vector<uint8_t> D2DBSSessionFsm::make_keepalive() {
    std::vector<uint8_t> packet;
    packet.push_back(static_cast<uint8_t>(D2DBSPacketType::KEEPALIVE));
    packet.push_back(2);  // Size: type(1) + size(1)
    return packet;
}

} // namespace pvpgn::protocol::d2dbs
