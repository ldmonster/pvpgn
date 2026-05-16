#pragma once
#include "core/result.hpp"
#include <cstdint>
#include <span>
#include <functional>
#include <string>
#include <vector>

namespace pvpgn::protocol::d2dbs {

// D2DBS packet types (from legacy d2dbs code)
enum class D2DBSPacketType : uint8_t {
    // D2CS → D2DBS
    CHARLOGINREQ    = 0x01,
    CHARSAVEREQ     = 0x02,
    CHARLADDERREQ   = 0x03,
    CHARLISTREQ     = 0x04,
    CHARCREATEREQ   = 0x05,
    CHARDELREQ      = 0x06,
    
    // D2DBS → D2CS
    CHARLOGINREPLY  = 0x11,
    CHARSAVEREPLY   = 0x12,
    CHARLADDERREPLY = 0x13,
    CHARLISTREPLY   = 0x14,
    CHARCREATEREPLY = 0x15,
    CHARDELREPLY    = 0x16,
    
    // Keepalive
    KEEPALIVE       = 0xFF,
};

enum class D2DBSSessionState {
    connected,
    authenticated,
    disconnected
};

struct D2DBSCharLoginRequest {
    std::string account_name;
    std::string char_name;
    uint32_t client_token;
};

struct D2DBSCharSaveRequest {
    std::string account_name;
    std::string char_name;
    std::vector<uint8_t> save_data;
};

struct D2DBSCharListRequest {
    std::string account_name;
};

struct D2DBSCharCreateRequest {
    std::string account_name;
    std::string char_name;
    uint8_t char_class;
    bool is_expansion;
    bool is_hardcore;
};

struct D2DBSCharDeleteRequest {
    std::string account_name;
    std::string char_name;
};

struct D2DBSFsmCallbacks {
    std::function<core::Result<std::vector<uint8_t>, core::Error>(const D2DBSCharLoginRequest&)> on_char_login;
    std::function<core::Result<void, core::Error>(const D2DBSCharSaveRequest&)> on_char_save;
    std::function<core::Result<std::vector<std::string>, core::Error>(const D2DBSCharListRequest&)> on_char_list;
    std::function<core::Result<void, core::Error>(const D2DBSCharCreateRequest&)> on_char_create;
    std::function<core::Result<void, core::Error>(const D2DBSCharDeleteRequest&)> on_char_delete;
    std::function<void()> on_disconnect;
};

class D2DBSSessionFsm {
public:
    explicit D2DBSSessionFsm(D2DBSFsmCallbacks callbacks);
    
    core::Result<size_t, core::Error> feed(std::span<const uint8_t> data);
    
    D2DBSSessionState state() const noexcept { return state_; }
    
    static std::vector<uint8_t> make_char_login_reply(uint8_t result, std::span<const uint8_t> save_data);
    static std::vector<uint8_t> make_char_save_reply(uint8_t result);
    static std::vector<uint8_t> make_char_list_reply(const std::vector<std::string>& chars);
    static std::vector<uint8_t> make_char_create_reply(uint8_t result);
    static std::vector<uint8_t> make_char_delete_reply(uint8_t result);
    static std::vector<uint8_t> make_keepalive();

private:
    D2DBSSessionState state_ = D2DBSSessionState::connected;
    D2DBSFsmCallbacks callbacks_;
    std::vector<uint8_t> buffer_;
    
    core::Result<void, core::Error> dispatch(D2DBSPacketType type, std::span<const uint8_t> payload);
    core::Result<void, core::Error> handle_char_login(std::span<const uint8_t> payload);
    core::Result<void, core::Error> handle_char_save(std::span<const uint8_t> payload);
    core::Result<void, core::Error> handle_char_list(std::span<const uint8_t> payload);
    core::Result<void, core::Error> handle_char_create(std::span<const uint8_t> payload);
    core::Result<void, core::Error> handle_char_delete(std::span<const uint8_t> payload);
};

} // namespace pvpgn::protocol::d2dbs
