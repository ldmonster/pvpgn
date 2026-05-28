// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file wol_session_factory.hpp
/// WOL (Westwood Online) protocol session factory.
/// Handles Westwood Studios game clients (C&C, Red Alert, etc.)

#include "core/bytes.hpp"
#include "core/result.hpp"
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <cstdint>

namespace pvpgn::integration::wol {

// WOL (Westwood Online) protocol session
// Handles Westwood Studios game clients (C&C, Red Alert, etc.)
class WolSession {
public:
    enum class State { connected, authenticated, in_lobby, in_game, disconnected };
    
    explicit WolSession(std::string session_id);
    
    // Feed raw bytes from client.
    // R212: takes canonical core::ByteView (std::span<const std::byte>) at the
    // integration boundary; internal buffer remains uint8_t-typed.
    core::Result<std::vector<uint8_t>, core::Error> feed(core::ByteView data);
    
    State state() const noexcept { return state_; }
    const std::string& session_id() const noexcept { return session_id_; }

private:
    std::string session_id_;
    State state_ = State::connected;
    std::vector<uint8_t> buffer_;
    
    // WOL packet types
    enum class WolPacketType : uint8_t {
        LOGIN_REQUEST   = 0x01,
        LOGIN_REPLY     = 0x02,
        CHANNEL_LIST    = 0x03,
        JOIN_CHANNEL    = 0x04,
        CHAT_MESSAGE    = 0x05,
        GAME_LIST       = 0x06,
        CREATE_GAME     = 0x07,
        JOIN_GAME       = 0x08,
        LEAVE_GAME      = 0x09,
        PING            = 0x0A,
        PONG            = 0x0B,
    };
    
    core::Result<std::vector<uint8_t>, core::Error> handle_login(std::span<const uint8_t> payload);
    core::Result<std::vector<uint8_t>, core::Error> handle_ping();
    static std::vector<uint8_t> make_reply(WolPacketType type, std::span<const uint8_t> payload);
};

class WolSessionFactory {
public:
    static std::unique_ptr<WolSession> create(std::string session_id);
};

}  // namespace pvpgn::integration::wol
