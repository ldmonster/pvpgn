// SPDX-License-Identifier: GPL-2.0-or-later
/// @file connection_fsm.cpp
/// ConnectionFsm — thin coordinator: dispatch() + close() + private helpers.
///
/// Per-state handler implementations live in focused sub-TUs:
///   connection_fsm_connecting.cpp     — on_auth_info, on_auth_check, on_logon_request
///   connection_fsm_authenticating.cpp — on_auth_accountlogon, on_auth_accountlogonproof
///   connection_fsm_loggedin.cpp       — on_enter_chat
///   connection_fsm_inchannel.cpp      — on_join_channel, on_chat_command,
///                                       on_leave_channel, on_start_game, on_join_game
///   connection_fsm_ingame.cpp         — on_leave_game, on_d2_char_select,
///                                       on_warcraft_general
///
/// Shared packet helpers (write_le32, read_le32, read_cstring, build_chat_event)
/// live in connection_fsm_internal.hpp.

#include "application/connection/connection_fsm.hpp"

#include <span>
#include <vector>

#include "core/error.hpp"

#include "connection_fsm_internal.hpp"

namespace pvpgn::application::connection {

using namespace detail;

// ---------------------------------------------------------------------------
// ConnectionFsm — public API
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::dispatch(std::uint8_t packet_id,
                                        std::span<const std::byte> payload) {
    if (state_ == ConnectionState::Disconnecting) {
        // Silently drop all packets once we are shutting down.
        return core::ok();
    }

    switch (packet_id) {
        // Keepalive — legal in every non-Disconnecting state
        case sid::kNull:
            return core::ok();

        // Ping echo — legal in every non-Disconnecting state
        case sid::kPing: {
            // Echo the 4-byte cookie verbatim
            std::vector<std::byte> body;
            body.reserve(4);
            const std::uint32_t cookie = read_le32(payload, 0);
            write_le32(body, cookie);
            return ctx_.send_packet(sid::kPing,
                                    std::span<const std::byte>{body});
        }

        // --- Connecting state ---
        case sid::kAuthInfo:
            return on_auth_info(payload);

        case sid::kLogonRequest:
        case sid::kLogonRequest2:
            return on_logon_request(payload);

        // --- Authenticating state ---
        case sid::kAuthCheck:
            return on_auth_check(payload);

        case sid::kAuthAccountLogon:
            return on_auth_accountlogon(payload);

        case sid::kAuthAccountLogonProof:
            return on_auth_accountlogonproof(payload);

        // --- LoggedIn state ---
        case sid::kEnterChat:
            return on_enter_chat(payload);

        // --- InChannel state ---
        case sid::kJoinChannel:
            return on_join_channel(payload);

        case sid::kChatCommand:
            return on_chat_command(payload);

        case sid::kLeaveChannel:
            return on_leave_channel(payload);

        case sid::kStartGame1:
        case sid::kStartGame3:
            return on_start_game(payload);

        case sid::kJoinGame:
            return on_join_game(payload);

        // --- InGame state ---
        case sid::kCloseGame:
            return on_leave_game(payload);

        // --- D2 character select (R287) ---
        case sid::kD2CharSelect:
            return on_d2_char_select(payload);

        // --- WAR3 route token (R288) ---
        case sid::kWarcraftGeneral:
            return on_warcraft_general(payload);

        default:
            // Unknown / unimplemented packet — silently ignore.
            // This is intentional: the strangler-fig bridge may handle it,
            // or it may be a future SID not yet migrated.
            return core::ok();
    }
}

void ConnectionFsm::close() {
    state_ = ConnectionState::Disconnecting;
    ctx_.close();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

core::Status<> ConnectionFsm::reject(const char* reason) {
    state_ = ConnectionState::Disconnecting;
    ctx_.close();
    return core::fail(core::Error{core::StatusCode::InvalidArgument, reason});
}

void ConnectionFsm::clear_pending_nls() noexcept {
    pending_nls_ctx_.reset();
    pending_nls_username_.reset();
    pending_nls_client_key_.reset();
    pending_nls_account_id_.reset();
}

}  // namespace pvpgn::application::connection
