#pragma once
#include "core/result.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace pvpgn::protocol::d2cs {

// D2CS packet types (from legacy d2cs code)
enum class D2CSPacketType : uint8_t {
    // Client → D2CS
    LOGINREQ        = 0x01,
    CHARLOGINREQ    = 0x07,
    CREATEGAMEREQ   = 0x09,
    JOINGAMEREQ     = 0x0B,
    GAMEINFOREQ     = 0x0D,
    CHARLADDERREQ   = 0x11,
    CHARLISTREQ     = 0x13,
    CHARCREATEREQ   = 0x17,
    CHARDELREQ      = 0x19,
    
    // D2CS → Client
    LOGINREPLY      = 0x02,
    CHARLOGINREPLY  = 0x08,
    CREATEGAMEREPLY = 0x0A,
    JOINGAMEREPLY   = 0x0C,
    GAMEINFOREPLY   = 0x0E,
    CHARLADDERREPLY = 0x12,
    CHARLISTREPLY   = 0x14,
    CHARCREATEREPLY = 0x18,
    CHARDELREPLY    = 0x1A,
    
    // D2CS → D2GS
    GAMETOKEN       = 0x30,
    CHARINFO        = 0x31,
    
    // D2GS → D2CS
    GAMECREATED     = 0x40,
    GAMEJOINED      = 0x41,
    GAMELEFT        = 0x42,
    CHARLOCK        = 0x43,
    CHARUNLOCK      = 0x44,
};

enum class D2CSSessionState {
    connected,
    authenticating,
    authenticated,
    in_game,
    disconnected
};

struct D2CSPacketHeader {
    uint16_t length;
    uint8_t  type;
};

struct D2CSLoginRequest {
    uint32_t account_id;
    uint32_t session_key;
    std::string account_name;
    std::string char_name;
};

struct D2CSCharLoginRequest {
    std::string account_name;
    std::string char_name;
    uint32_t client_token;
};

struct D2CSCreateGameRequest {
    std::string game_name;
    std::string game_password;
    std::string game_description;
    uint8_t difficulty;
    bool is_expansion;
};

struct D2CSJoinGameRequest {
    std::string game_name;
    std::string game_password;
};

// Callbacks for FSM events
struct D2CSFsmCallbacks {
    std::function<core::Result<void, core::Error>(const D2CSLoginRequest&)> on_login;
    std::function<core::Result<void, core::Error>(const D2CSCharLoginRequest&)> on_char_login;
    std::function<core::Result<uint32_t, core::Error>(const D2CSCreateGameRequest&)> on_create_game;
    std::function<core::Result<void, core::Error>(const D2CSJoinGameRequest&)> on_join_game;
    std::function<void()> on_disconnect;
};

class D2CSSessionFsm {
public:
    explicit D2CSSessionFsm(D2CSFsmCallbacks callbacks);
    
    // Feed raw bytes; returns bytes consumed
    core::Result<size_t, core::Error> feed(const uint8_t* data, size_t len);
    
    D2CSSessionState state() const noexcept { return state_; }
    
    // Build reply packets
    static std::vector<uint8_t> make_login_reply(uint8_t result_code);
    static std::vector<uint8_t> make_char_login_reply(uint8_t result_code);
    static std::vector<uint8_t> make_create_game_reply(uint8_t result_code, uint32_t token);
    static std::vector<uint8_t> make_join_game_reply(uint8_t result_code, std::string_view gs_addr, uint16_t gs_port);
    static std::vector<uint8_t> make_char_list_reply(const std::vector<std::string>& char_names);

private:
    D2CSSessionState state_ = D2CSSessionState::connected;
    D2CSFsmCallbacks callbacks_;
    std::vector<uint8_t> buffer_;
    
    core::Result<void, core::Error> dispatch(D2CSPacketType type, const uint8_t* payload, size_t len);
    core::Result<void, core::Error> handle_login(const uint8_t* payload, size_t len);
    core::Result<void, core::Error> handle_char_login(const uint8_t* payload, size_t len);
    core::Result<void, core::Error> handle_create_game(const uint8_t* payload, size_t len);
    core::Result<void, core::Error> handle_join_game(const uint8_t* payload, size_t len);
};

} // namespace pvpgn::protocol::d2cs
