// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file irc_session_factory.hpp
/// IRC protocol session factory for IRC client access to BNet.

#include "core/result.hpp"
#include <memory>
#include <string>
#include <vector>
#include <span>
#include <functional>
#include <cstdint>

namespace pvpgn::integration::irc {

// IRC protocol session for IRC client access to BNet
class IrcSession {
public:
    enum class State { connected, registering, registered, disconnected };
    
    using OutputCallback = std::function<void(std::string_view)>;
    
    explicit IrcSession(std::string session_id, OutputCallback output_cb);
    
    // Feed raw bytes from client
    core::Result<void, core::Error> feed(std::span<const uint8_t> data);
    
    State state() const noexcept { return state_; }
    const std::string& session_id() const noexcept { return session_id_; }
    const std::string& nickname() const noexcept { return nickname_; }

private:
    std::string session_id_;
    State state_ = State::connected;
    OutputCallback output_cb_;
    std::string line_buffer_;
    std::string nickname_;
    std::string username_;
    std::string realname_;
    bool nick_set_ = false;
    bool user_set_ = false;
    
    void send(std::string_view line);
    void handle_line(std::string_view line);
    
    // IRC command handlers
    void handle_nick(std::string_view params);
    void handle_user(std::string_view params);
    void handle_join(std::string_view params);
    void handle_privmsg(std::string_view params);
    void handle_ping(std::string_view params);
    void handle_quit(std::string_view params);
    void handle_list(std::string_view params);
    
    // IRC reply codes
    static constexpr int RPL_WELCOME    = 1;
    static constexpr int RPL_YOURHOST   = 2;
    static constexpr int RPL_CREATED    = 3;
    static constexpr int RPL_MYINFO     = 4;
    static constexpr int RPL_LIST       = 322;
    static constexpr int RPL_LISTEND    = 323;
    static constexpr int RPL_NAMREPLY   = 353;
    static constexpr int RPL_ENDOFNAMES = 366;
    static constexpr int ERR_NOSUCHNICK = 401;
    static constexpr int ERR_NOSUCHCHANNEL = 403;
    static constexpr int ERR_NONICKNAMEGIVEN = 431;
    static constexpr int ERR_NICKNAMEINUSE = 433;
    
    void send_numeric(int code, std::string_view params);
    void try_complete_registration();
};

class IrcSessionFactory {
public:
    static std::unique_ptr<IrcSession> create(
        std::string session_id,
        IrcSession::OutputCallback output_cb
    );
};

}  // namespace pvpgn::integration::irc
