// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/irc/irc_session_factory.hpp"

#include <span>
#include <sstream>
#include <algorithm>

namespace pvpgn::integration::irc {

IrcSession::IrcSession(std::string session_id, OutputCallback output_cb)
    : session_id_(std::move(session_id)), output_cb_(std::move(output_cb)) {}

core::Result<void, core::Error> IrcSession::feed(std::span<const uint8_t> data) {
    for (uint8_t byte : data) {
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

void IrcSession::send(std::string_view line) {
    if (output_cb_) {
        std::string msg(line);
        msg += "\r\n";
        output_cb_(msg);
    }
}

void IrcSession::handle_line(std::string_view line) {
    if (line.empty()) {
        return;
    }
    
    // Parse IRC command format: COMMAND [params...]
    std::istringstream iss(std::string(line));
    std::string command;
    iss >> command;
    
    // Convert command to uppercase
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    
    // Get remaining parameters
    std::string params;
    std::getline(iss, params);
    if (!params.empty() && params[0] == ' ') {
        params = params.substr(1);
    }
    
    // Dispatch command
    if (command == "NICK") {
        handle_nick(params);
    } else if (command == "USER") {
        handle_user(params);
    } else if (command == "JOIN") {
        handle_join(params);
    } else if (command == "PRIVMSG") {
        handle_privmsg(params);
    } else if (command == "PING") {
        handle_ping(params);
    } else if (command == "QUIT") {
        handle_quit(params);
    } else if (command == "LIST") {
        handle_list(params);
    } else {
        // Unknown command, ignore
    }
}

void IrcSession::handle_nick(std::string_view params) {
    if (params.empty()) {
        send_numeric(ERR_NONICKNAMEGIVEN, "");
        return;
    }
    
    std::string new_nick(params);
    // Remove trailing whitespace
    new_nick.erase(new_nick.find_last_not_of(" \t\r\n") + 1);
    
    if (new_nick == nickname_) {
        return;  // No change
    }
    
    nickname_ = new_nick;
    nick_set_ = true;
    try_complete_registration();
}

void IrcSession::handle_user(std::string_view params) {
    std::istringstream iss(std::string(params));
    iss >> username_;
    
    // Skip mode and unused
    std::string unused;
    iss >> unused >> unused;
    
    // Get realname (rest of line after colon)
    std::getline(iss, realname_);
    if (!realname_.empty() && realname_[0] == ':') {
        realname_ = realname_.substr(1);
    }
    
    user_set_ = true;
    try_complete_registration();
}

void IrcSession::try_complete_registration() {
    if (nick_set_ && user_set_ && state_ == State::registering) {
        state_ = State::registered;
        
        // Send welcome messages
        send_numeric(RPL_WELCOME, "Welcome to PvPGN IRC");
        send_numeric(RPL_YOURHOST, "Your host is pvpgn.local");
        send_numeric(RPL_CREATED, "This server was created today");
        send_numeric(RPL_MYINFO, "pvpgn.local 1.0 iw nt");
    } else if (nick_set_ && user_set_ && state_ == State::connected) {
        state_ = State::registering;
        try_complete_registration();
    }
}

void IrcSession::handle_join(std::string_view params) {
    if (state_ != State::registered) {
        return;
    }
    
    std::string channel(params);
    channel.erase(channel.find_last_not_of(" \t\r\n") + 1);
    
    if (channel.empty()) {
        return;
    }
    
    // Send join confirmation
    send(":" + nickname_ + " JOIN " + channel);
    
    // Send channel topic
    send_numeric(RPL_NAMREPLY, "= " + channel + " :" + nickname_);
    send_numeric(RPL_ENDOFNAMES, channel + " :End of NAMES list");
}

void IrcSession::handle_privmsg(std::string_view params) {
    if (state_ != State::registered) {
        return;
    }
    
    std::istringstream iss(std::string(params));
    std::string target;
    iss >> target;
    
    std::string message;
    std::getline(iss, message);
    if (!message.empty() && message[0] == ' ') {
        message = message.substr(1);
    }
    if (!message.empty() && message[0] == ':') {
        message = message.substr(1);
    }
    
    // Echo message back (in real implementation, route to target)
    send(":" + nickname_ + " PRIVMSG " + target + " :" + message);
}

void IrcSession::handle_ping(std::string_view params) {
    send("PONG :" + std::string(params));
}

void IrcSession::handle_quit(std::string_view params) {
    state_ = State::disconnected;
    send("ERROR :Closing connection");
}

void IrcSession::handle_list(std::string_view params) {
    if (state_ != State::registered) {
        return;
    }
    
    // Send channel list
    send_numeric(RPL_LIST, "General 5 :General discussion");
    send_numeric(RPL_LIST, "Games 3 :Game announcements");
    send_numeric(RPL_LISTEND, ":End of LIST");
}

void IrcSession::send_numeric(int code, std::string_view params) {
    std::string msg = ":" + session_id_ + " " + std::to_string(code) + " " + nickname_ + " " + std::string(params);
    send(msg);
}

std::unique_ptr<IrcSession> IrcSessionFactory::create(
    std::string session_id,
    IrcSession::OutputCallback output_cb) {
    return std::make_unique<IrcSession>(std::move(session_id), std::move(output_cb));
}

}  // namespace pvpgn::integration::irc
