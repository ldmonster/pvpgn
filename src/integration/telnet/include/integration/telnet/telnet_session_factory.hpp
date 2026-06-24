// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file telnet_session_factory.hpp
/// Telnet protocol session factory for text-based BNet access.

#include "core/bytes.hpp"
#include "core/result.hpp"
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <functional>
#include <cstdint>

namespace pvpgn::integration::telnet {

// Telnet protocol session for text-based BNet access
class TelnetSession {
public:
    enum class State { connected, authenticating, authenticated, disconnected };
    
    using OutputCallback = std::function<void(std::string_view)>;
    
    explicit TelnetSession(std::string session_id, OutputCallback output_cb);
    
    // Feed raw bytes from client.
    // Canonical core::ByteView at the integration boundary.
    core::Result<void, core::Error> feed(core::ByteView data);
    
    State state() const noexcept { return state_; }
    const std::string& session_id() const noexcept { return session_id_; }

private:
    std::string session_id_;
    State state_ = State::connected;
    OutputCallback output_cb_;
    std::string line_buffer_;
    std::string username_;
    
    void send(std::string_view text);
    void handle_line(std::string_view line);
    void handle_login_line(std::string_view line);
    void handle_command_line(std::string_view line);
    
    // Telnet negotiation
    static constexpr uint8_t IAC  = 0xFF;
    static constexpr uint8_t WILL = 0xFB;
    static constexpr uint8_t WONT = 0xFC;
    static constexpr uint8_t DO   = 0xFD;
    static constexpr uint8_t DONT = 0xFE;
    static constexpr uint8_t ECHO = 0x01;
    static constexpr uint8_t SGA  = 0x03;
};

class TelnetSessionFactory {
public:
    static std::unique_ptr<TelnetSession> create(
        std::string session_id,
        TelnetSession::OutputCallback output_cb
    );
};

}  // namespace pvpgn::integration::telnet
