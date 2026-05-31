// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm.cpp
/// BnetFsm — thin coordinator.
///
/// R-split: fsm.cpp (914 LOC) split by state group into focused sub-TUs:
///   fsm/fsm_auth.cpp  — Init → AuthInfoReceived → LoggedIn + legacy OLS
///   fsm/fsm_chat.cpp  — InChat state: channel, chat, friends, realm, ladder
///   fsm/fsm_game.cpp  — InGame state: start/join/close game, map auth
///   fsm/fsm_clan.cpp  — Clan + ArrangedTeam handlers
///   fsm/fsm_misc.cpp  — Advisory / no-op handlers (keepalive, ads, etc.)
///
/// This file retains only:
///   reject()                — close the session and return an error
///   broadcast_chat_event()  — fan-out a ChatEvent to a set of sessions
///   handle()                — top-level dispatch via std::visit
///   require_clan_state()    — shared guard used by chat/game/clan sub-TUs

#include "protocol/bnet/fsm.hpp"

#include <span>
#include <variant>

#include "core/error.hpp"
#include "application/ports/message_router.hpp"
#include "protocol/bnet/codec.hpp"
#include "protocol/common/writer.hpp"

namespace pvpgn::protocol::bnet {

// ---------------------------------------------------------------------------
// require_clan_state — shared guard used by fsm_chat, fsm_game, fsm_clan
// ---------------------------------------------------------------------------
core::Status<> require_clan_state(BnetState s, const char* msg) {
    if (s != BnetState::InChat && s != BnetState::LoggedIn && s != BnetState::InGame) {
        return core::fail(core::Error{core::StatusCode::FailedPrecondition, msg});
    }
    return core::ok();
}

// ---------------------------------------------------------------------------
// BnetFsm coordinator methods
// ---------------------------------------------------------------------------

core::Status<> BnetFsm::reject(const char* reason) {
    state_ = BnetState::Closing;
    ctx_->close();
    return core::fail(core::Error{core::StatusCode::InvalidArgument, reason});
}

void BnetFsm::broadcast_chat_event(const ChatEvent& ev,
                                   std::span<const domain::SessionId> sessions) {
    if (!use_cases_.message_router || sessions.empty()) return;
    Writer w;
    w.begin_bnet_packet(0x0F);  // SID_CHATEVENT
    if (!encode(w, ev)) return;
    if (!w.finalize_bnet_packet()) return;
    auto bytes = w.take();
    (void)use_cases_.message_router->broadcast(
        sessions,
        std::span<const std::byte>{bytes.data(), bytes.size()});
}

core::Status<> BnetFsm::handle(const ClientMessage& msg) {
    if (state_ == BnetState::Closing) {
        return core::fail(core::Error{
            core::StatusCode::FailedPrecondition, "bnet fsm: closing"});
    }
    return std::visit([this](const auto& m) { return on(m); }, msg);
}

}  // namespace pvpgn::protocol::bnet
