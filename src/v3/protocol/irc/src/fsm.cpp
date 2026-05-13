// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol/irc/fsm.hpp"

#include <string>

#include "core/error.hpp"

namespace pvpgn::protocol::irc {

namespace {

Message numeric(std::string_view server, int code,
                std::string_view target, std::string_view text) {
    Message m;
    m.prefix = std::string{server};
    char buf[4];  // 3 digits + NUL
    buf[0] = static_cast<char>('0' + (code / 100) % 10);
    buf[1] = static_cast<char>('0' + (code / 10) % 10);
    buf[2] = static_cast<char>('0' + code % 10);
    buf[3] = '\0';
    m.command.assign(buf, 3);
    m.params.emplace_back(target);
    m.params.emplace_back(text);
    return m;
}

}  // namespace

core::Status<> IrcFsm::send_numeric(int code,
                                    std::string_view target,
                                    std::string_view text) {
    return ctx_->send(numeric(ctx_->server_name(), code, target, text));
}

core::Status<> IrcFsm::try_complete_registration() {
    if (!nick_.empty() && !user_.empty() && state_ == IrcState::Greeting) {
        state_ = IrcState::Registered;
        // 001 RPL_WELCOME
        std::string text = "Welcome to PvPGN, ";
        text += nick_;
        return send_numeric(1, nick_, text);
    }
    return core::ok();
}

core::Status<> IrcFsm::on_nick(const Message& m) {
    if (m.params.empty() || m.params[0].empty()) {
        return send_numeric(431, nick_.empty() ? "*" : nick_,
                            "No nickname given");
    }
    nick_ = m.params[0];
    return try_complete_registration();
}

core::Status<> IrcFsm::on_user(const Message& m) {
    // USER <user> <mode> <unused> :<realname>
    if (m.params.size() < 4) {
        return send_numeric(461, nick_.empty() ? "*" : nick_,
                            "USER :Not enough parameters");
    }
    user_ = m.params[0];
    return try_complete_registration();
}

core::Status<> IrcFsm::on_ping(const Message& m) {
    Message pong;
    pong.prefix  = std::string{ctx_->server_name()};
    pong.command = "PONG";
    pong.params.emplace_back(ctx_->server_name());
    if (!m.params.empty()) {
        pong.params.emplace_back(m.params[0]);
    }
    return ctx_->send(pong);
}

core::Status<> IrcFsm::on_join(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    if (m.params.empty() || m.params[0].empty()) {
        return send_numeric(461, nick_, "JOIN :Not enough parameters");
    }
    channel_ = m.params[0];
    state_   = IrcState::InChannel;

    // Echo back the JOIN with our user prefix, then RPL_ENDOFNAMES (366).
    Message echo;
    echo.prefix  = nick_;
    echo.command = "JOIN";
    echo.params.emplace_back(channel_);
    auto s = ctx_->send(echo);
    if (!s) return s;
    return send_numeric(366, channel_, "End of /NAMES list.");
}

core::Status<> IrcFsm::on_privmsg(const Message& m) {
    if (state_ != IrcState::Registered && state_ != IrcState::InChannel) {
        return send_numeric(451, nick_.empty() ? "*" : nick_,
                            "You have not registered");
    }
    if (m.params.size() < 2) {
        return send_numeric(411, nick_, "PRIVMSG :No recipient or text");
    }
    // Phase-5 dispatcher will forward to chat use-case. Skeleton just
    // acknowledges by echoing back with the server prefix so tests can
    // observe the path.
    Message echo;
    echo.prefix  = nick_;
    echo.command = "PRIVMSG";
    echo.params  = m.params;
    return ctx_->send(echo);
}

core::Status<> IrcFsm::on_quit(const Message&) {
    state_ = IrcState::Closing;
    ctx_->close();
    return core::ok();
}

core::Status<> IrcFsm::handle(const Message& msg) {
    if (state_ == IrcState::Closing) {
        return core::fail(core::Error{
            core::StatusCode::FailedPrecondition, "irc fsm: closing"});
    }
    const auto& c = msg.command;
    if (c == "PING")    return on_ping(msg);
    if (c == "NICK")    return on_nick(msg);
    if (c == "USER")    return on_user(msg);
    if (c == "JOIN")    return on_join(msg);
    if (c == "PRIVMSG") return on_privmsg(msg);
    if (c == "QUIT")    return on_quit(msg);

    // 421 ERR_UNKNOWNCOMMAND
    return send_numeric(421, nick_.empty() ? "*" : nick_,
                        c + " :Unknown command");
}

}  // namespace pvpgn::protocol::irc
