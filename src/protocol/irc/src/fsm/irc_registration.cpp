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
            auto s = send_numeric(432, nick_ + " :Erroneous nickname");
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
            auto s = send_numeric(464, ":Password incorrect");
            if (!s) return s;
            state_ = IrcState::Closing;
            ctx_->close();
            return core::ok();
        }

        // Store the resolved account ID for use-case calls.
        account_id_ = result.value().id;
    }

    state_ = IrcState::Registered;
    // The original (handle_irc_welcome) sends the FULL registration burst, not
    // just RPL_WELCOME: 001 welcome, 002 yourhost, 003 created, 004 myinfo,
    // 005 isupport, then the MOTD (375 [/ 372 lines] / 376). Many IRC clients
    // block until they see 004 RPL_MYINFO or the end-of-MOTD, so emitting only
    // 001 left a real client waiting. Match the numeric sequence — the host/
    // version/time text is environment-specific and not byte-compared.
    const std::string sv{ctx_->server_name()};
    if (auto s = send_numeric(1, ":Welcome to the " + sv + " IRC Network " + nick_);
        !s) return s;
    if (auto s = send_numeric(2, ":Your host is " + sv + ", running pvpgn-v3");
        !s) return s;
    if (auto s = send_numeric(3, ":This server was created at startup"); !s) return s;
    if (auto s = send_numeric(4, sv + " pvpgn-v3 - -"); !s) return s;
    if (auto s = send_numeric(5,
            "NICKLEN=15 TOPICLEN=255 CHANNELLEN=255 CHANTYPES=#& NETWORK=" + sv +
            " IRCD=pvpgn :are supported by this server"); !s) return s;
    // MOTD framing (375 start / 376 end). v3 has no MOTD content line (372);
    // the original, with a configured MOTD file, additionally sends 372.
    if (auto s = send_numeric(375, ":- " + sv + " Message of the day -"); !s) return s;
    return send_numeric(376, ":End of /MOTD command.");
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
        return send_numeric(461, "PASS :Not enough parameters");
    }

    pending_password_ = m.params[0];

    // If NICK and USER have already been received, attempt registration now.
    return try_complete_registration();
}

core::Status<> IrcFsm::on_nick(const Message& m) {
    if (m.params.empty() || m.params[0].empty()) {
        // 431 ERR_NONICKNAMEGIVEN
        return send_numeric(431, ":No nickname given");
    }
    nick_ = m.params[0];
    return try_complete_registration();
}

core::Status<> IrcFsm::on_user(const Message& m) {
    // USER <user> <mode> <unused> :<realname>
    if (m.params.size() < 4) {
        // 461 ERR_NEEDMOREPARAMS
        return send_numeric(461, "USER :Not enough parameters");
    }
    user_ = m.params[0];
    return try_complete_registration();
}

}  // namespace pvpgn::protocol::irc
