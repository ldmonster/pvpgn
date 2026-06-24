// SPDX-License-Identifier: GPL-2.0-or-later
/// @file irc_registration.cpp
/// IrcFsm — registration-phase handlers.
///
/// Handles the IRC connection handshake (Greeting → Registered):
///   try_complete_registration() — fires when NICK + USER (+ PASS) are all received
///   on_pass()  — stores the password; triggers registration attempt
///   on_nick()  — stores the nick;     triggers registration attempt
///   on_user()  — stores the user;     triggers registration attempt

#include "protocol/irc/fsm.hpp"

#include "application/auth/login_user.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/user_name.hpp"
#include "fsm/irc_internal.hpp"

namespace pvpgn::protocol::irc {

// ===========================================================================
// Registration
// ===========================================================================

core::Status<> IrcFsm::try_complete_registration() {
    if (nick_.empty() || user_.empty() || state_ != IrcState::Greeting)
        return core::ok();

    // When a LoginUser use-case is wired in, we also require a PASS before
    // completing registration.  If the password hasn't arrived yet, wait.
    if (login_user_ != nullptr && !pending_password_.has_value())
        return core::ok();

    if (login_user_ != nullptr) {
        // --- OLS authentication path ---
        auto name_result = domain::UserName::parse(nick_);
        if (!name_result) {
            // 432 ERR_ERRONEUSNICKNAME
            auto s = send_numeric(432, effective_nick(),
                                  nick_ + " :Erroneous nickname");
            if (!s) return s;
            state_ = IrcState::Closing;
            ctx_->close();
            return core::ok();
        }

        application::auth::LoginRequest req{
            std::move(name_result).value(),
            domain::BNHash{},
            domain::ClientTag{},
            domain::IpAddress{},
            domain::SessionId{}
        };

        auto result = login_user_->execute(std::move(req));
        if (!result) {
            // 464 ERR_PASSWDMISMATCH
            auto s = send_numeric(464, effective_nick(), "Password incorrect");
            if (!s) return s;
            state_ = IrcState::Closing;
            ctx_->close();
            return core::ok();
        }

        // Store the resolved account ID for use-case calls.
        account_id_ = result.value().id;
    }

    state_ = IrcState::Registered;
    // 001 RPL_WELCOME
    std::string text = "Welcome to PvPGN, ";
    text += nick_;
    return send_numeric(1, nick_, text);
}

// ---------------------------------------------------------------------------
// PASS handler
// ---------------------------------------------------------------------------

core::Status<> IrcFsm::on_pass(const Message& m) {
    // Silently ignore PASS after registration is complete.
    if (state_ != IrcState::Greeting)
        return core::ok();

    if (m.params.empty() || m.params[0].empty()) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, effective_nick(),
                            "PASS :Not enough parameters");
    }

    pending_password_ = m.params[0];

    // If NICK and USER have already been received, attempt registration now.
    return try_complete_registration();
}

core::Status<> IrcFsm::on_nick(const Message& m) {
    if (m.params.empty() || m.params[0].empty()) {
        // 431 ERR_NONICKNAMEGIVEN
        return send_numeric(431, effective_nick(), "No nickname given");
    }
    nick_ = m.params[0];
    return try_complete_registration();
}

core::Status<> IrcFsm::on_user(const Message& m) {
    // USER <user> <mode> <unused> :<realname>
    if (m.params.size() < 4) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, effective_nick(),
                            "USER :Not enough parameters");
    }
    user_ = m.params[0];
    return try_complete_registration();
}

}  // namespace pvpgn::protocol::irc
