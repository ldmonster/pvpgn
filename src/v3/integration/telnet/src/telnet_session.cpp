// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/telnet/telnet_session_factory.hpp"

#include <span>
#include <algorithm>

namespace pvpgn::integration::telnet {

TelnetSession::TelnetSession(std::string session_id, OutputCallback output_cb)
    : session_id_(std::move(session_id)), output_cb_(std::move(output_cb)) {
    // Send initial telnet negotiation
    std::vector<uint8_t> init_seq = {
        IAC, WILL, ECHO,
        IAC, WILL, SGA,
        IAC, DO, SGA
    };
    send("Welcome to PvPGN Telnet Interface\r\n");
    send("Username: ");
}

core::Result<void, core::Error> TelnetSession::feed(std::span<const uint8_t> data) {
    for (uint8_t byte : data) {
        // Handle telnet escape sequences
        if (byte == IAC) {
            // Skip telnet negotiation for now
            continue;
        }
        
        // Handle line endings
        if (byte == '\r' || byte == '\n') {
            if (!line_buffer_.empty()) {
                handle_line(line_buffer_);
                line_buffer_.clear();
            }
            continue;
        }
        
        // Accumulate printable characters
        if (byte >= 32 && byte < 127) {
            line_buffer_ += static_cast<char>(byte);
        }
    }
    
    return core::Result<void, core::Error>{};
}

void TelnetSession::send(std::string_view text) {
    if (output_cb_) {
        output_cb_(text);
    }
}

void TelnetSession::handle_line(std::string_view line) {
    switch (state_) {
        case State::connected:
            handle_login_line(line);
            break;
        case State::authenticating:
            handle_login_line(line);
            break;
        case State::authenticated:
            handle_command_line(line);
            break;
        case State::disconnected:
            break;
    }
}

void TelnetSession::handle_login_line(std::string_view line) {
    if (state_ == State::connected) {
        // First line is username
        username_ = std::string(line);
        state_ = State::authenticating;
        send("Password: ");
    } else if (state_ == State::authenticating) {
        // Second line is password (in real implementation, validate it)
        state_ = State::authenticated;
        send("\r\nAuthenticated. Type 'help' for commands.\r\n");
        send("> ");
    }
}

void TelnetSession::handle_command_line(std::string_view line) {
    if (line == "help") {
        send("Available commands:\r\n");
        send("  help     - Show this help\r\n");
        send("  quit     - Disconnect\r\n");
        send("  channels - List channels\r\n");
        send("> ");
    } else if (line == "quit") {
        state_ = State::disconnected;
        send("Goodbye!\r\n");
    } else if (line == "channels") {
        send("Available channels:\r\n");
        send("  General\r\n");
        send("  Games\r\n");
        send("> ");
    } else if (!line.empty()) {
        send("Unknown command. Type 'help' for available commands.\r\n");
        send("> ");
    }
}

std::unique_ptr<TelnetSession> TelnetSessionFactory::create(
    std::string session_id,
    TelnetSession::OutputCallback output_cb) {
    return std::make_unique<TelnetSession>(std::move(session_id), std::move(output_cb));
}

}  // namespace pvpgn::integration::telnet
