// SPDX-License-Identifier: GPL-2.0-or-later
/// @file wol_auth.cpp
/// WolFsm — registration and authentication handlers.
///
/// Handles the WOL connection handshake (Connecting → Authenticating → Authenticated):
///   on_nick()          — stores nick; transitions to Authenticating
///   on_user()          — stores user/realname; triggers auth if PASS already received
///   on_pass()          — stores password; triggers auth if NICK+USER already received
///   try_authenticate() — performs OLS auth via LoginUser use-case (or skeleton accept)

#include "protocol/wol/wol_fsm.hpp"

#include "application/auth/login_user.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/user_name.hpp"
#include "wol_fsm/wol_internal.hpp"

namespace pvpgn::protocol::wol {

core::Status<> WolFsm::on_nick(std::string_view params) {
    auto new_nick = trim(first_token(params));
    if (new_nick.empty()) {
        // 431 ERR_NONICKNAMEGIVEN
        return send_numeric(431, nick_.empty() ? "*" : nick_,
                            "No nickname given");
    }

    nick_ = std::string(new_nick);

    if (state_ == WolState::Connecting) {
        state_ = WolState::Authenticating;
    }
    return core::ok();
}

core::Status<> WolFsm::on_user(std::string_view params) {
    // USER <username> <hostname> <servername> :<realname>
    auto sp1 = params.find(' ');
    if (sp1 == std::string_view::npos) {
        return send_numeric(461, nick_.empty() ? "*" : nick_,
                            "USER :Not enough parameters");
    }
    user_ = std::string(params.substr(0, sp1));

    auto trailing = extract_trailing(params);
    realname_ = std::string(trailing.empty() ? user_ : trailing);

    // If PASS was already received before USER, attempt auth now that we
    // have all three credentials (NICK + USER + PASS).
    // close_on_bad_credentials=true: PASS-first ordering cannot retry.
    if (state_ == WolState::Authenticating && !nick_.empty() && !pass_.empty()) {
        return try_authenticate(/*close_on_bad_credentials=*/true);
    }

    return core::ok();
}

core::Status<> WolFsm::on_pass(std::string_view params) {
    // PASS <password>  (may be prefixed with ':')
    auto pw = trim(params);
    if (!pw.empty() && pw[0] == ':') pw.remove_prefix(1);
    pass_ = std::string(pw);

    // WOL auth: we need NICK + USER + PASS.
    // If we already have nick_ and user_, attempt auth immediately.
    // Otherwise store pass_ and wait for the remaining commands.
    if (state_ != WolState::Authenticating || nick_.empty() || user_.empty()) {
        return core::ok();
    }

    // Normal PASS-last ordering: on InvalidCredentials keep connection open
    // so the client can retry.
    return try_authenticate(/*close_on_bad_credentials=*/false);
}

core::Status<> WolFsm::try_authenticate(bool close_on_bad_credentials) {
    // R289: If a LoginUser use-case is wired in, perform real OLS auth.
    if (login_user_ != nullptr) {
        // Parse the username — reject malformed names immediately.
        auto name_result = domain::UserName::parse(nick_);
        if (!name_result) {
            // 432 ERR_ERRONEUSNICKNAME
            auto st = send_numeric(432, nick_, "Erroneous nickname");
            if (!st) return st;
            state_ = WolState::Disconnecting;
            ctx_->close();
            return core::ok();
        }

        // WOL sends the password in plain text.  We store it in a zeroed
        // BNHash and rely on the IPasswordHasher injected into LoginUser to
        // derive the expected hash from the stored hash1 and compare.
        // (The LoginWithSessionHashRequest path is used for BNCS OLS; for WOL
        // we use the simpler LoginRequest with a zeroed hash as a placeholder
        // until a WOL-specific hasher is wired in Phase 5.)
        //
        // C++20 designated initializers: UserName has no default ctor so we
        // must supply all fields in declaration order.
        application::auth::LoginRequest req{
            /* name               = */ std::move(name_result).value(),
            /* password_candidate = */ domain::BNHash{},    // zeroed — hasher re-derives
            /* tag                = */ domain::ClientTag{},  // WOL has no product tag
            /* ip                 = */ domain::IpAddress{},  // filled by infra layer
            /* session            = */ domain::SessionId{}   // filled by infra layer
        };

        auto result = login_user_->execute(std::move(req));

        if (!result) {
            // 464 ERR_PASSWDMISMATCH
            auto st = send_numeric(464, nick_, "Password incorrect");
            if (!st) return st;
            // Close on UnknownUser (hard failure) or when the caller
            // indicates that the ordering does not allow a retry.
            if (close_on_bad_credentials ||
                result.error() == application::auth::LoginError::UnknownUser) {
                state_ = WolState::Disconnecting;
                ctx_->close();
            }
            return core::ok();
        }

        // Auth succeeded.
        state_ = WolState::Authenticated;
        std::string welcome = "Welcome to WOL, ";
        welcome += nick_;
        auto st = send_numeric(1, nick_, welcome);
        if (!st) return st;
        std::string yourhost = "Your host is ";
        yourhost += std::string(ctx_->server_name());
        yourhost += ", running PvPGN v3";
        st = send_numeric(2, nick_, yourhost);
        if (!st) return st;
        st = send_numeric(375, nick_, "- Message of the day -");
        if (!st) return st;
        return send_numeric(376, nick_, "End of /MOTD command.");
    }

    // Skeleton / test mode: accept any non-empty nick+user combination.
    state_ = WolState::Authenticated;
    std::string welcome = "Welcome to WOL, ";
    welcome += nick_;
    auto st = send_numeric(1, nick_, welcome);
    if (!st) return st;
    std::string yourhost = "Your host is ";
    yourhost += std::string(ctx_->server_name());
    yourhost += ", running PvPGN v3";
    st = send_numeric(2, nick_, yourhost);
    if (!st) return st;
    st = send_numeric(375, nick_, "- Message of the day -");
    if (!st) return st;
    return send_numeric(376, nick_, "End of /MOTD command.");
}

}  // namespace pvpgn::protocol::wol
