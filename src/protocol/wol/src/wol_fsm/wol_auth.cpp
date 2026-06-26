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

#include <cstdlib>
#include <string>

#include "application/auth/create_account.hpp"
#include "application/auth/login_user.hpp"
#include "application/auth/wol_credential_store.hpp"
#include "domain/connection/peer_address_store.hpp"
#include "domain/connection/ports.hpp"
#include "domain/identity/ports.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/locale.hpp"
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
        return send_needmoreparams("USER");
    }
    user_ = std::string(params.substr(0, sp1));

    auto trailing = extract_trailing(params);
    realname_ = std::string(trailing.empty() ? user_ : trailing);

    // Native Westwood Online flow: USER is the welcome/auth trigger. A WOL
    // client sends CVERS/VERCHK/APGAR/NICK before USER and never sends PASS, so
    // when the WOL auth collaborators are wired and an APGAR token is present we
    // authenticate the Westwood way (auto-create / verbatim-compare) here.
    if (auth_.complete() && !nick_.empty() && !apgar_.empty()) {
        return try_wol_authenticate();
    }

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
    // If a LoginUser use-case is wired in, perform real OLS auth.
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
        // until a WOL-specific hasher is wired in.)
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
        return send_welcome_and_motd();
    }

    // Skeleton / test mode: accept any non-empty nick+user combination.
    state_ = WolState::Authenticated;
    return send_welcome_and_motd();
}

// ===========================================================================
// send_welcome_and_motd — the post-auth 001/002 + MOTD (375…376) sequence
// ===========================================================================
core::Status<> WolFsm::send_welcome_and_motd() {
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

// ===========================================================================
// Native Westwood Online auth handlers (CVERS / VERCHK / APGAR / USER)
// ===========================================================================

core::Status<> WolFsm::on_cvers(std::string_view params) {
    // CVERS <oldvernum> <SKU> — the SKU identifies the WOL product.
    auto p = trim(params);
    auto sp = p.find(' ');
    if (sp == std::string_view::npos) {
        return send_needmoreparams("CVERS");
    }
    auto sku_str = first_token(trim(p.substr(sp + 1)));
    wol_sku_ = std::atoi(std::string(sku_str).c_str());
    return core::ok();
}

core::Status<> WolFsm::on_verchk(std::string_view params) {
    // VERCHK <SKU> <version> — reply that no update is required (379 NONREQ).
    auto p = trim(params);
    auto sp = p.find(' ');
    if (sp == std::string_view::npos) {
        return send_needmoreparams("VERCHK");
    }
    int sku = std::atoi(std::string(first_token(p)).c_str());
    if (sku != 0) wol_sku_ = sku;
    // Original wire text: "none none none 1 <SKU> NONREQ".
    std::string text = "none none none 1 ";
    text += std::to_string(wol_sku_);
    text += " NONREQ";
    return send_numeric(379, nick_.empty() ? "*" : nick_, text);
}

core::Status<> WolFsm::on_apgar(std::string_view params) {
    auto token = trim(first_token(trim(params)));
    if (token.empty()) {
        return send_needmoreparams("APGAR");
    }
    apgar_ = std::string(token);
    return core::ok();
}

// ===========================================================================
// try_wol_authenticate — auto-create on first login, verbatim compare after
// ===========================================================================
core::Status<> WolFsm::try_wol_authenticate() {
    auto name = domain::UserName::parse(nick_);
    if (!name) {
        // 432 ERR_ERRONEUSNICKNAME
        auto st = send_numeric(432, nick_, "Erroneous nickname");
        if (!st) return st;
        state_ = WolState::Disconnecting;
        ctx_->close();
        return core::ok();
    }

    domain::AccountId acct_id{0};
    auto existing = auth_.account_reader->find_by_name(name.value());
    if (!existing) {
        // First login: auto-create the account. WOL never checks the OLS
        // password, so a default (zeroed) hash is fine; the APGAR token is the
        // real credential and is stored in the WOL credential store.
        application::auth::CreateAccountRequest req{
            .username      = name.value(),
            .password_hash = domain::BNHash{},
            .email         = "",
            .locale        = domain::Locale{},
            .client_tag    = domain::ClientTag{},
            .peer_ip       = domain::IpAddress{},
        };
        auto created = auth_.create_account->execute(req);
        if (!created) {
            // Mirror the original's "account creating failed" → bad-login reply.
            return send_numeric(378, nick_,
                "You have specified an invalid password for that nickname.");
        }
        acct_id = created.value();
        auth_.wol_store->store(nick_,
            application::auth::WolCredentials{apgar_, acct_id});
    } else {
        acct_id = existing.value().id();
        auto creds = auth_.wol_store->find(nick_);
        if (!creds) {
            // Account exists but has no WOL token yet (e.g. created via another
            // protocol): adopt the supplied token, as the original does.
            auth_.wol_store->store(nick_,
                application::auth::WolCredentials{apgar_, acct_id});
        } else if (creds->apgar != apgar_) {
            // 378 RPL_BAD_LOGIN — wrong APGAR. Keep the link open for retry.
            return send_numeric(378, nick_,
                "You have specified an invalid password for that nickname.");
        }
        // else: token matches — proceed.
    }

    // Attach the session if a registry is wired. kick-old-login (the original's
    // default, matching BNCS w49/w50): if this account already has a live
    // session, detach it and close that old connection, then attach this one.
    // Previously the attach failure was ignored, so a second login for the same
    // account left BOTH sessions alive — the old one a ghost the oracle would
    // have kicked. The welcome is still sent regardless so a benign attach race
    // never blocks login.
    if (auth_.session_registry) {
        if (auto old = auth_.session_registry->session_for(acct_id);
            old && old.value() != session_id_) {
            auth_.session_registry->detach(old.value());
            if (message_router_) {
                (void)message_router_->disconnect(old.value());
            }
        }
        (void)auth_.session_registry->attach(session_id_, acct_id);
    }
    // Register this account's peer IP so USERIP/STARTG can report it.
    if (peer_store_ && !peer_ip_.empty()) {
        peer_store_->set(acct_id, peer_ip_);
    }
    account_id_ = acct_id;
    state_      = WolState::Authenticated;
    return send_welcome_and_motd();
}

}  // namespace pvpgn::protocol::wol
