// SPDX-License-Identifier: GPL-2.0-or-later
#include "integration/legacy_bnetd/install_v3_handlers.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include "application/auth/change_password.hpp"
#include "application/auth/login_user.hpp"
#include "core/clock.hpp"
#include "core/logging.hpp"
#include "domain/shared/bn_hash.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/shared/ip_address.hpp"
#include "domain/shared/user_name.hpp"
#include "infra/inmemory/event_bus.hpp"
#include "infra/inmemory/session_registry.hpp"
#include "infra/legacy_crypto/bnet_session_hasher.hpp"
#include "integration/legacy_bnetd/bridge_logger.hpp"
#include "integration/legacy_bnetd/change_password_bridge.hpp"
#include "integration/legacy_bnetd/legacy_account_repository.hpp"
#include "integration/legacy_bnetd/login_user_bridge.hpp"
#include "integration/legacy_bnetd/send_packet_bridge.hpp"
#include "integration/legacy_bnetd/init_conn_bridge.hpp"

#include "common/setup_before.h"
#include "common/bn_type.h"
#include "common/bnet_protocol.h"
#include "common/packet.h"
#include "bnetd/account.h"
#include "bnetd/account_wrap.h"
#include "bnetd/connection.h"
#include "common/setup_after.h"

namespace pvpgn::integration::legacy_bnetd {

namespace {

/// Live composition root for v3 ChangePassword. Owns the singleton
/// dependencies so the handler can outlive any one invocation.
struct ChangePasswordContext {
    infra::legacy_crypto::BnetSessionHasher  hasher;
    LegacyAccountRepository                  accounts;
    infra::inmemory::InMemoryEventBus        bus;
    application::auth::ChangePasswordUseCase use_case{
        accounts, bus, hasher};
};

std::atomic<bool>                       g_cp_installed{false};
std::unique_ptr<ChangePasswordContext>  g_cp_ctx;

/// Pull the 20-byte BNHash out of a `bn_int[5]` field. The wire
/// format is the same one `bnhash_to_hash` reads, but we copy raw
/// bytes -- the bnethash transcript on the *server* side puts the
/// 20 bytes back in little-endian order, so byte equality is what
/// we need.
domain::BNHash read_bnhash(const ::pvpgn::bn_int (&field)[5]) noexcept {
    domain::BNHash::Bytes bytes{};
    static_assert(sizeof(field) == 20,
                  "bn_int[5] must be 20 bytes wide on the wire");
    std::memcpy(bytes.data(), &field[0], 20);
    return domain::BNHash{bytes};
}

int change_password_handler(void* conn_ptr,
                            void const* packet_body,
                            unsigned int packet_size) noexcept {
    using ::pvpgn::bnetd::t_connection;
    if (conn_ptr == nullptr || packet_body == nullptr) return 0;
    if (packet_size < sizeof(::pvpgn::t_client_changepassreq)) return 0;

    auto* c       = static_cast<t_connection*>(conn_ptr);
    auto const* p = static_cast<::pvpgn::t_client_changepassreq const*>(
        packet_body);

    // Extract the username from the trailing string field.
    char const* name_cstr =
        reinterpret_cast<char const*>(p) +
        sizeof(::pvpgn::t_client_changepassreq);
    // Bound check: ensure there's at least one NUL within the
    // declared packet size.
    bool name_terminated = false;
    for (unsigned int i = sizeof(::pvpgn::t_client_changepassreq);
         i < packet_size; ++i) {
        if (reinterpret_cast<char const*>(p)[i] == '\0') {
            name_terminated = true;
            break;
        }
    }
    if (!name_terminated) {
        const std::string size_str = std::to_string(packet_size);
        bridge_log_kv(core::LogLevel::Warn, "v3.changepass",
                      "rejecting unterminated username",
                      {{"size", size_str}});
        return 0;  // fall through to legacy
    }

    const std::uint32_t ticks =
        static_cast<std::uint32_t>(::pvpgn::bn_int_get(p->ticks));
    const std::uint32_t sessionkey =
        static_cast<std::uint32_t>(::pvpgn::bn_int_get(p->sessionkey));

    auto name = domain::UserName::parse(std::string_view{name_cstr});
    if (!name) return 0;  // malformed -> legacy

    application::auth::ChangePasswordWithSessionHashRequest req{
        name.value(),
        read_bnhash(p->oldpassword_hash2),
        ticks,
        sessionkey,
        read_bnhash(p->newpassword_hash1),
    };

    auto result = g_cp_ctx->use_case.execute(req);
    const bool success = static_cast<bool>(result);

    // Build the legacy SERVER_CHANGEPASSACK packet so we can short-
    // circuit the legacy handler entirely.
    auto* rpacket = ::pvpgn::packet_create(::pvpgn::packet_class_bnet);
    if (rpacket == nullptr) return 0;  // legacy path will retry
    ::pvpgn::packet_set_size(
        rpacket, sizeof(::pvpgn::t_server_changepassack));
    ::pvpgn::packet_set_type(rpacket, SERVER_CHANGEPASSACK);
    ::pvpgn::bn_int_set(
        &rpacket->u.server_changepassack.message,
        success ? SERVER_CHANGEPASSACK_MESSAGE_SUCCESS
                : SERVER_CHANGEPASSACK_MESSAGE_FAIL);
    ::pvpgn::bnetd::conn_push_outqueue(c, rpacket);
    ::pvpgn::packet_del_ref(rpacket);

    const std::string_view user_view{name_cstr};
    bridge_log_kv(core::LogLevel::Info, "v3.changepass",
                  success ? "handled (success)" : "handled (failure)",
                  {{"user", user_view}});
    return 1;  // v3 owned the whole request
}

}  // namespace

void install_change_password_handler() {
    bool expected = false;
    if (!g_cp_installed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;  // already installed
    }
    g_cp_ctx = std::make_unique<ChangePasswordContext>();
    set_change_password_handler(&change_password_handler);
    bridge_log_kv(core::LogLevel::Info, "v3.changepass",
                  "v3 handler installed", {});
}

namespace {

/// Live composition root for v3 LoginUser (31b).
///
/// Why the hasher is captured by-value here: `LoginUser`'s
/// hash2-aware constructor stores a `const IPasswordHasher*`, so
/// the dependency must outlive the use-case. Putting the hasher
/// alongside the use-case in a single struct gives us that
/// guarantee without a separate static.
struct LoginContext {
    infra::legacy_crypto::BnetSessionHasher hasher;
    LegacyAccountRepository                accounts;
    infra::inmemory::InMemorySessionRegistry sessions;
    infra::inmemory::InMemoryEventBus      bus;
    core::SystemClock                      clock;
    application::auth::LoginUser           use_case{
        accounts, sessions, bus, clock, hasher};
};

std::atomic<bool>                  g_login_installed{false};
std::unique_ptr<LoginContext>      g_login_ctx;

/// 31b: real handler -- short-circuits *failure* verdicts for
/// CLIENT_LOGINREQ1 / CLIENT_LOGINREQ2 by emitting the legacy
/// reply packet ourselves and returning 1 ("v3 owned it").
/// CLIENT_LOGINREQ_W3 uses NLS/SRP (no `password_hash2`) so the
/// v3 use-case cannot evaluate it -- those always fall through.
/// On the *Ok* verdict we also fall through, because the legacy
/// success path performs many side effects (account binding,
/// presence broadcast, OCN updates, etc.) that the v3 use-case
/// does not yet replicate. The use-case is invoked purely for its
/// verdict; persistence side-effects are limited to event-bus
/// publication, which is in-memory.
int login_user_handler(void* conn_ptr,
                       void const* req_body,
                       unsigned int req_size) noexcept {
    using ::pvpgn::bnetd::t_connection;
    if (conn_ptr == nullptr || req_body == nullptr) return 0;

    auto* c          = static_cast<t_connection*>(conn_ptr);
    auto const* pkt  = static_cast<::pvpgn::t_packet const*>(req_body);
    const unsigned int type = ::pvpgn::packet_get_type(pkt);

    // Only loginreq1/2 are in scope for the hash2 path. Anything
    // else falls through unconditionally.
    if (type != CLIENT_LOGINREQ1 && type != CLIENT_LOGINREQ2) {
        return 0;
    }
    if (req_size < sizeof(::pvpgn::t_client_loginreq1)) {
        return 0;  // malformed -> let legacy log/reject it
    }

    // The two layouts are byte-identical: same header + ticks +
    // sessionkey + password_hash2[5] + trailing NUL-terminated
    // username. Read via `client_loginreq1` for either SID.
    auto const& body = pkt->u.client_loginreq1;

    char const* name_cstr =
        reinterpret_cast<char const*>(pkt) +
        sizeof(::pvpgn::t_client_loginreq1);
    bool name_terminated = false;
    for (unsigned int i = sizeof(::pvpgn::t_client_loginreq1);
         i < req_size; ++i) {
        if (reinterpret_cast<char const*>(pkt)[i] == '\0') {
            name_terminated = true;
            break;
        }
    }
    if (!name_terminated) {
        return 0;  // let legacy emit its own error
    }

    auto name = domain::UserName::parse(std::string_view{name_cstr});
    if (!name) return 0;

    const std::uint32_t ticks      =
        static_cast<std::uint32_t>(::pvpgn::bn_int_get(body.ticks));
    const std::uint32_t sessionkey =
        static_cast<std::uint32_t>(::pvpgn::bn_int_get(body.sessionkey));

    application::auth::LoginWithSessionHashRequest req{
        name.value(),
        read_bnhash(body.password_hash2),
        ticks,
        sessionkey,
        domain::ClientTag{},     // not used by current use-case
        domain::IpAddress{},     // ditto
        domain::SessionId{},     // ditto
    };

    auto result = g_login_ctx->use_case.execute(req);
    if (result) {
        // Success: legacy path still owns binding the account to
        // the connection, presence broadcast, etc. Fall through.
        bridge_log_kv(core::LogLevel::Info, "v3.login",
                      "verdict ok -- falling through to legacy",
                      {{"user", std::string_view{name_cstr}}});
        return 0;
    }

    // Failure verdicts: we own them. Build the legacy reply
    // packet and emit the closest matching message code.
    const auto err = result.error();
    const bool is_loginreq2 = (type == CLIENT_LOGINREQ2);

    auto* rpacket = ::pvpgn::packet_create(::pvpgn::packet_class_bnet);
    if (rpacket == nullptr) return 0;

    if (is_loginreq2) {
        ::pvpgn::packet_set_size(
            rpacket, sizeof(::pvpgn::t_server_loginreply2));
        ::pvpgn::packet_set_type(rpacket, SERVER_LOGINREPLY2);
        std::uint32_t code = SERVER_LOGINREPLY2_MESSAGE_BADPASS;
        switch (err) {
        case application::auth::LoginError::UnknownUser:
            code = SERVER_LOGINREPLY2_MESSAGE_NONEXIST; break;
        case application::auth::LoginError::InvalidCredentials:
            code = SERVER_LOGINREPLY2_MESSAGE_BADPASS;  break;
        case application::auth::LoginError::Locked:
        case application::auth::LoginError::Banned:
            code = SERVER_LOGINREPLY2_MESSAGE_LOCKED;   break;
        default:
            code = SERVER_LOGINREPLY2_MESSAGE_BADPASS;  break;
        }
        ::pvpgn::bn_int_set(
            &rpacket->u.server_loginreply2.message, code);
    } else {
        ::pvpgn::packet_set_size(
            rpacket, sizeof(::pvpgn::t_server_loginreply1));
        ::pvpgn::packet_set_type(rpacket, SERVER_LOGINREPLY1);
        // loginreply1 has only FAIL/SUCCESS -- everything non-ok
        // is FAIL.
        ::pvpgn::bn_int_set(
            &rpacket->u.server_loginreply1.message,
            SERVER_LOGINREPLY1_MESSAGE_FAIL);
    }
    ::pvpgn::bnetd::conn_push_outqueue(c, rpacket);
    ::pvpgn::packet_del_ref(rpacket);

    char const* err_name = "internal";
    switch (err) {
    case application::auth::LoginError::UnknownUser:        err_name = "unknown_user"; break;
    case application::auth::LoginError::InvalidCredentials: err_name = "bad_password"; break;
    case application::auth::LoginError::Locked:             err_name = "locked"; break;
    case application::auth::LoginError::Banned:             err_name = "banned"; break;
    case application::auth::LoginError::AlreadyLoggedIn:    err_name = "already_logged_in"; break;
    case application::auth::LoginError::PersistenceFailed:  err_name = "persistence"; break;
    case application::auth::LoginError::MustChangePassword: err_name = "must_change_pw"; break;
    case application::auth::LoginError::Internal:           err_name = "internal"; break;
    }
    bridge_log_kv(core::LogLevel::Info, "v3.login",
                  "handled (failure)",
                  {{"user",   std::string_view{name_cstr}},
                   {"verdict", std::string_view{err_name}},
                   {"req",     is_loginreq2 ? std::string_view{"loginreq2"}
                                            : std::string_view{"loginreq1"}}});
    return 1;
}

}  // namespace

void install_login_user_handler() {
    bool expected = false;
    if (!g_login_installed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }
    g_login_ctx = std::make_unique<LoginContext>();
    set_login_user_handler(&login_user_handler);
    bridge_log_kv(core::LogLevel::Info, "v3.login",
                  "v3 login handler installed", {});
}

namespace {
std::atomic<bool> g_send_packet_installed{false};
}  // namespace

void install_send_packet_handler() {
    bool expected = false;
    if (!g_send_packet_installed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }
    install_legacy_send_packet_handler();
    bridge_log_kv(core::LogLevel::Info, "v3.send_packet",
                  "v3 send_packet handler installed", {});
}

namespace {
std::atomic<bool> g_init_conn_installed{false};
}  // namespace

void install_init_conn_apply_handler() {
    bool expected = false;
    if (!g_init_conn_installed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel)) {
        return;
    }
    install_legacy_init_conn_apply_handler();
    bridge_log_kv(core::LogLevel::Info, "v3.init_conn",
                  "v3 init_conn apply handler installed", {});
}

}  // namespace pvpgn::integration::legacy_bnetd
