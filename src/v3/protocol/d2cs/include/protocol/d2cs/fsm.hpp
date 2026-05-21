// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file fsm.hpp
/// D2CS client-session finite-state machine.
///
/// Packet type codes and wire layout are taken from
/// `src/v3/protocol/d2cs/include/protocol/d2cs/wire_types.hpp`
/// (which mirrors `src/common/d2cs_protocol.h`).
///
/// The FSM owns a reassembly buffer and processes complete D2CS packets.
/// All multi-byte integers are little-endian on the wire.
///
/// Header layout (3 bytes):
///   [0..1]  uint16_t  total packet length (including header)
///   [2]     uint8_t   packet type
///
/// Payload follows immediately after the 3-byte header.

#include "core/result.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pvpgn::protocol::d2cs {

// ---------------------------------------------------------------------------
// Packet type codes (client → D2CS, matching wire_types.hpp)
// ---------------------------------------------------------------------------

enum class D2CSPacketType : uint8_t {
    // Client → D2CS
    LOGINREQ            = 0x01,  ///< Account login request
    CREATECHARREQ       = 0x02,  ///< Create character request
    CREATEGAMEREQ       = 0x03,  ///< Create game request
    JOINGAMEREQ         = 0x04,  ///< Join game request
    GAMELISTREQ         = 0x05,  ///< Game list request
    GAMEINFOREQ         = 0x06,  ///< Game info request
    CHARLOGINREQ        = 0x07,  ///< Character login request
    DELETECHARREQ       = 0x0A,  ///< Delete character request
    LADDERREQ           = 0x11,  ///< Ladder request
    MOTDREQ             = 0x12,  ///< MOTD request
    CANCELCREATEGAME    = 0x13,  ///< Cancel create-game request
    CHARLADDERREQ       = 0x16,  ///< Character ladder request
    CHARLISTREQ         = 0x17,  ///< Character list request
    CONVERTCHARREQ      = 0x18,  ///< Convert character request
    CHARLISTREQ110      = 0x19,  ///< Character list request (1.10+)

    // D2CS → Client (reply codes)
    LOGINREPLY          = 0x01,  ///< Account login reply
    CREATECHARREPLY     = 0x02,  ///< Create character reply
    CREATEGAMEREPLY     = 0x03,  ///< Create game reply
    JOINGAMEREPLY       = 0x04,  ///< Join game reply
    GAMELISTREPLY       = 0x05,  ///< Game list reply
    GAMEINFOREPLY       = 0x06,  ///< Game info reply
    CHARLOGINREPLY      = 0x07,  ///< Character login reply
    DELETECHARREPLY     = 0x0A,  ///< Delete character reply
    LADDERREPLY         = 0x11,  ///< Ladder reply
    MOTDREPLY           = 0x12,  ///< MOTD reply
    CREATEGAMEWAIT      = 0x14,  ///< Create-game queue position
    CHARLISTREPLY       = 0x17,  ///< Character list reply
    CONVERTCHARREPLY    = 0x18,  ///< Convert character reply
    CHARLISTREPLY110    = 0x19,  ///< Character list reply (1.10+)
};

// ---------------------------------------------------------------------------
// Session state
// ---------------------------------------------------------------------------

enum class D2CSSessionState {
    connected,      ///< TCP connected, no login yet
    authenticating, ///< LOGINREQ sent, waiting for bnetd auth
    authenticated,  ///< Account authenticated, no character selected
    in_game,        ///< Character in a game
    disconnected    ///< Session terminated
};

// ---------------------------------------------------------------------------
// Packet header
// ---------------------------------------------------------------------------

struct D2CSPacketHeader {
    uint16_t length;  ///< Total packet length including this header
    uint8_t  type;    ///< Packet type code
};

// ---------------------------------------------------------------------------
// Request structs (parsed from wire)
// ---------------------------------------------------------------------------

struct D2CSLoginRequest {
    uint32_t    seqno;        ///< Sequence number
    uint32_t    session_key;  ///< Session key from bnetd
    std::string account_name; ///< Null-terminated account name
    std::string char_name;    ///< Null-terminated character name
};

struct D2CSCharLoginRequest {
    uint32_t    seqno;        ///< Sequence number
    uint32_t    char_class;   ///< Character class
    uint32_t    char_level;   ///< Character level
    uint32_t    char_status;  ///< Character status flags
    std::string account_name; ///< Null-terminated account name
    std::string char_name;    ///< Null-terminated character name
};

struct D2CSCreateGameRequest {
    uint32_t    seqno;            ///< Sequence number
    uint8_t     difficulty;       ///< Game difficulty (0=Normal, 1=Nightmare, 2=Hell)
    uint8_t     hardcore;         ///< Hardcore flag
    uint8_t     expansion;        ///< Expansion flag
    std::string game_name;        ///< Null-terminated game name
    std::string game_password;    ///< Null-terminated game password
    std::string game_description; ///< Null-terminated game description
};

struct D2CSJoinGameRequest {
    uint32_t    seqno;         ///< Sequence number
    std::string game_name;     ///< Null-terminated game name
    std::string game_password; ///< Null-terminated game password
};

struct D2CSGameListRequest {
    uint32_t seqno;       ///< Sequence number
    uint32_t game_type;   ///< Game type filter
};

struct D2CSGameInfoRequest {
    uint32_t    seqno;     ///< Sequence number
    std::string game_name; ///< Null-terminated game name
};

struct D2CSCreateCharRequest {
    uint32_t    seqno;      ///< Sequence number
    uint8_t     char_class; ///< Character class
    uint8_t     char_flags; ///< Character flags (hardcore/expansion)
    std::string char_name;  ///< Null-terminated character name
};

struct D2CSDeleteCharRequest {
    uint32_t    seqno;     ///< Sequence number
    std::string char_name; ///< Null-terminated character name
};

struct D2CSCharListRequest {
    uint32_t seqno; ///< Sequence number
};

struct D2CSMotdRequest {
    uint32_t seqno; ///< Sequence number
};

struct D2CSConvertCharRequest {
    uint32_t    seqno;     ///< Sequence number
    std::string char_name; ///< Null-terminated character name
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

/// All callbacks return `core::Result<void, core::Error>`.
/// Returning a failure causes `feed()` to propagate the error.
struct D2CSFsmCallbacks {
    std::function<core::Result<void, core::Error>(const D2CSLoginRequest&)>      on_login;
    std::function<core::Result<void, core::Error>(const D2CSCharLoginRequest&)>  on_char_login;
    std::function<core::Result<void, core::Error>(const D2CSCreateGameRequest&)> on_create_game;
    std::function<core::Result<void, core::Error>(const D2CSJoinGameRequest&)>   on_join_game;
    std::function<core::Result<void, core::Error>(const D2CSGameListRequest&)>   on_game_list;
    std::function<core::Result<void, core::Error>(const D2CSGameInfoRequest&)>   on_game_info;
    std::function<core::Result<void, core::Error>(const D2CSCreateCharRequest&)> on_create_char;
    std::function<core::Result<void, core::Error>(const D2CSDeleteCharRequest&)> on_delete_char;
    std::function<core::Result<void, core::Error>(const D2CSCharListRequest&)>   on_char_list;
    std::function<core::Result<void, core::Error>(const D2CSMotdRequest&)>       on_motd;
    std::function<core::Result<void, core::Error>(const D2CSConvertCharRequest&)>on_convert_char;
    std::function<core::Result<void, core::Error>()>                             on_cancel_create_game;
    std::function<void()>                                                        on_disconnect;
};

// ---------------------------------------------------------------------------
// D2CSSessionFsm
// ---------------------------------------------------------------------------

class D2CSSessionFsm {
public:
    explicit D2CSSessionFsm(D2CSFsmCallbacks callbacks);

    /// Feed raw bytes from the TCP stream.
    /// Returns the number of bytes consumed (may be less than `len` if a
    /// partial packet is buffered).  Returns a failure if a packet is
    /// malformed or a callback returns an error.
    [[nodiscard]] core::Result<size_t, core::Error> feed(const uint8_t* data, size_t len);

    /// Current FSM state.
    [[nodiscard]] D2CSSessionState state() const noexcept { return state_; }

    // -----------------------------------------------------------------------
    // Packet builders (D2CS → client)
    // -----------------------------------------------------------------------

    /// Build a LOGINREPLY packet.
    /// @param result_code  0x00 = success, 0x0C = bad password
    [[nodiscard]] static std::vector<uint8_t> make_login_reply(uint32_t result_code);

    /// Build a CHARLOGINREPLY packet.
    /// @param result_code  0x00 = success, 0x01 = failed, 0x46 = not found
    [[nodiscard]] static std::vector<uint8_t> make_char_login_reply(uint32_t result_code);

    /// Build a CREATEGAMEREPLY packet.
    /// @param seqno        Sequence number echoed from request
    /// @param game_id      Assigned game ID (0 on failure)
    /// @param result_code  0x00 = success, 0x01 = failed
    [[nodiscard]] static std::vector<uint8_t> make_create_game_reply(
        uint32_t seqno, uint32_t game_id, uint32_t result_code);

    /// Build a JOINGAMEREPLY packet.
    /// @param seqno        Sequence number echoed from request
    /// @param game_id      Game ID (0 on failure)
    /// @param gs_ip        Game-server IPv4 address (host byte order)
    /// @param token        Join token
    /// @param result_code  0x00 = success
    [[nodiscard]] static std::vector<uint8_t> make_join_game_reply(
        uint32_t seqno, uint32_t game_id, uint32_t gs_ip,
        uint32_t token, uint32_t result_code);

    /// Build a CHARLISTREPLY packet.
    /// @param char_names   List of character names
    [[nodiscard]] static std::vector<uint8_t> make_char_list_reply(
        const std::vector<std::string>& char_names);

    /// Build a CREATECHARREPLY packet.
    /// @param result_code  0x00 = success, 0x01 = failed, 0x14 = already exists
    [[nodiscard]] static std::vector<uint8_t> make_create_char_reply(uint32_t result_code);

    /// Build a DELETECHARREPLY packet.
    /// @param result_code  0x00 = success, 0x01 = failed
    [[nodiscard]] static std::vector<uint8_t> make_delete_char_reply(uint32_t result_code);

    /// Build a MOTDREPLY packet.
    /// @param message  MOTD text (null-terminated on wire)
    [[nodiscard]] static std::vector<uint8_t> make_motd_reply(std::string_view message);

    /// Build a CREATEGAMEWAIT packet (queue position notification).
    /// @param position  Queue position (1-based)
    [[nodiscard]] static std::vector<uint8_t> make_create_game_wait(uint32_t position);

    /// Build a CONVERTCHARREPLY packet.
    /// @param result_code  0x00 = success, 0x01 = failed
    [[nodiscard]] static std::vector<uint8_t> make_convert_char_reply(uint32_t result_code);

private:
    D2CSSessionState  state_ = D2CSSessionState::connected;
    D2CSFsmCallbacks  callbacks_;
    std::vector<uint8_t> buffer_;

    static constexpr size_t kHeaderSize = 3;  ///< length(2) + type(1)
    static constexpr size_t kMinPacketLen = kHeaderSize;

    [[nodiscard]] core::Result<void, core::Error> dispatch(
        D2CSPacketType type, const uint8_t* payload, size_t len);

    [[nodiscard]] core::Result<void, core::Error> handle_login(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_char_login(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_create_game(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_join_game(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_game_list(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_game_info(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_create_char(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_delete_char(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_char_list(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_motd(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_cancel_create_game(
        const uint8_t* payload, size_t len);
    [[nodiscard]] core::Result<void, core::Error> handle_convert_char(
        const uint8_t* payload, size_t len);

    /// Read a null-terminated string from `buf[offset..]`.
    /// Advances `offset` past the null terminator.
    /// Returns false if no null terminator is found within `len` bytes.
    [[nodiscard]] static bool read_cstring(
        const uint8_t* buf, size_t len, size_t& offset, std::string& out);

    /// Read a little-endian uint32_t from `buf[offset..]`.
    /// Advances `offset` by 4.  Returns false if fewer than 4 bytes remain.
    [[nodiscard]] static bool read_u32le(
        const uint8_t* buf, size_t len, size_t& offset, uint32_t& out);

    /// Write a little-endian uint32_t into a vector.
    static void push_u32le(std::vector<uint8_t>& v, uint32_t val);

    /// Write a 3-byte packet header (length LE + type) into a vector.
    static void push_header(std::vector<uint8_t>& v, uint16_t total_len, D2CSPacketType type);
};

} // namespace pvpgn::protocol::d2cs
