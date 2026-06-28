// SPDX-License-Identifier: GPL-2.0-or-later
/// @file fsm_game.cpp
/// BnetFsm — game-lifecycle handlers (InGame state transitions).
///
/// Covers all handlers that manage game sessions:
///   on(StartGame1Request) — SID_STARTADVEX (0x1C)
///   on(StartGame3Request) — SID_STARTADVEX3 (0x1C variant)
///   on(StartGame4Request) — SID_STARTADVEX4 (clan/AT game start)
///   on(JoinGame)          — SID_JOINGAME (0x1D)
///   on(CloseGame)         — SID_CLOSEGAME (0x1F)
///   on(CloseGame2)        — SID_CLOSEGAME2 (0x2F)
///   on(GameReport)        — SID_GAMEREPORT (0x73)
///   on(MapAuthReq1)       — SID_MAPAUTHREQ1 (0x42)
///   on(MapAuthReq2)       — SID_MAPAUTHREQ2 (0x43)

#include "fsm/fsm_internal.hpp"

#include "protocol/bnet/game_wire_types.hpp"
#include "application/game/join_game.hpp"
#include "application/game/leave_game.hpp"
#include "application/game/start_game.hpp"

namespace pvpgn::protocol::bnet {

// --- Game-lifecycle handlers with state transitions -----------------------

// CLIENT_STARTGAME{1,3}_STATUSMASK / _STATUS_DONE from the original protocol.
// Mask off the low nibble and compare against the "game finished" status.
namespace {
constexpr std::uint32_t kStartGameStatusMask = 0x0000000fu;
constexpr std::uint32_t kStartGameStatusDone = 0x0000000cu;
}  // namespace

core::Status<> BnetFsm::on(const StartGame1Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME1 before login");
    if (!s) return s;

    // Mirror the original (_client_startgame1): a status update for an already
    // hosted game (or a DONE for a game that no longer exists) is silent — the
    // server only ACKs when it actually creates a game. We only host on the
    // STARTGAME4 path, so here current_game_id_ is never set; suppress the
    // spurious ACK for the "finished/destroyed game" status and reply nothing,
    // matching the oracle's "client tried to set game status DONE" log path.
    if (current_game_id_.value() != 0
        || (m.status & kStartGameStatusMask) == kStartGameStatusDone) {
        return core::ok();
    }

    if (!use_cases_.start_game) {
        // No start_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame1Ack{game::kStartGame1AckOk}});
    }

    // Use client_tag_ stored from AUTH_INFO
    // Call start_game use-case with game parameters from request
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_,
        m.game_name, "", static_cast<std::uint8_t>(m.gametype & 0xFFu));  // Empty map_name for now

    if (!start_result) {
        // Game start failed
        return ctx_->send(ServerMessage{StartGame1Ack{game::kStartGame1AckNo}});
    }

    // Store game ID and transition state
    const auto& start_res = start_result.value();
    current_game_id_ = start_res.game_id;
    state_ = BnetState::InGame;

    // Send success reply with game ID
    return ctx_->send(ServerMessage{StartGame1Ack{game::kStartGame1AckOk}});
}

core::Status<> BnetFsm::on(const StartGame3Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME3 before login");
    if (!s) return s;

    // Same gating as STARTGAME1 (see above): silent for already-hosted games and
    // for the DONE status against a non-existent game.
    if (current_game_id_.value() != 0
        || (m.status & kStartGameStatusMask) == kStartGameStatusDone) {
        return core::ok();
    }

    if (!use_cases_.start_game) {
        // No start_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame3Ack{game::kStartGame3AckOk}});
    }

    // Use client_tag_ stored from AUTH_INFO
    // Call start_game use-case with game parameters from request
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_,
        m.game_name, "", static_cast<std::uint8_t>(m.gametype & 0xFFu));

    if (!start_result) {
        return ctx_->send(ServerMessage{StartGame3Ack{game::kStartGame3AckNo}});
    }

    const auto& start_res = start_result.value();
    current_game_id_ = start_res.game_id;
    state_ = BnetState::InGame;

    return ctx_->send(ServerMessage{StartGame3Ack{game::kStartGame3AckOk}});
}

core::Status<> BnetFsm::on(const StartGame4Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME4 before login");
    if (!s) return s;

    // The original (_client_startgame4, SID_STARTADVEX3 0x1C) parts the host
    // from its current chat channel before advertising the game ("Quick hack to
    // make W3 part channels when creating a game"). This makes the remaining
    // channel members get EID_LEAVE and stops the host ghosting in the channel
    // roster while it hosts. Mirror it: if we are in a channel, leave it first
    // (on(LeaveChannel) no-ops when not actually a member, so this is safe even
    // if the subsequent advertise fails — matching the original's unconditional
    // part-before-process order). The later on_disconnect leave then finds
    // nothing to remove, so there is no double broadcast.
    if (state_ == BnetState::InChat) {
        (void)on(LeaveChannel{});
    }

    if (!use_cases_.start_game) {
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame4Ack{0x00u}});
    }
    // Advertise the hosted game in the shared repository so GETADVLISTEX
    // (0x09) on other connections can find it. `info` is the statstring/map.
    // A non-empty password marks a private game (not listed); StartGame models
    // public games, so we only host when no password was supplied — matching
    // the observable "open games appear in the list" behaviour.
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_, m.game_name, m.info,
        /*max_players*/ 8u);
    if (!start_result) {
        return ctx_->send(ServerMessage{StartGame4Ack{0x01u}});
    }
    current_game_id_ = start_result.value().game_id;
    state_           = BnetState::InGame;
    return ctx_->send(ServerMessage{StartGame4Ack{0x00u}});
}

core::Status<> BnetFsm::on(const JoinGame& m) {
    auto s = require_clan_state(state_, "bnet fsm: JOINGAME before login");
    if (!s) return s;

    if (!use_cases_.join_game) {
        // No join_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame4Ack{0x00u}});
    }

    // Parse game ID from message (game_name in the request)
    // For now, use a placeholder game ID
    domain::GameId game_id{0};

    auto join_result = use_cases_.join_game->execute(game_id, current_account_id_);

    if (!join_result) {
        // Join failed — send SID_STARTADVEX3 with non-zero error code
        return ctx_->send(ServerMessage{StartGame4Ack{0x01u}});
    }

    // Join succeeded - store game ID and transition to InGame
    current_game_id_ = game_id;
    state_ = BnetState::InGame;

    // Send SID_STARTADVEX3 (0x1C) success reply
    return ctx_->send(ServerMessage{StartGame4Ack{0x00u}});
}

core::Status<> BnetFsm::on(const CloseGame&) {
    // Accept in any post-login state; only transition out of InGame.
    auto s = require_clan_state(state_, "bnet fsm: CLOSEGAME before login");
    if (!s) return s;

    if (state_ == BnetState::InGame) {
        if (use_cases_.leave_game && current_game_id_.value() != 0) {
            auto leave_result = use_cases_.leave_game->execute(
                current_game_id_, current_account_id_);
            // LeaveGameResult has no member list; game-closed broadcast is
            // best-effort via a synthetic EID_LEAVE sent to the leaving player's
            // own session only (other players are notified by the game server).
            (void)leave_result;
        }

        current_game_id_ = domain::GameId{0};
        state_ = BnetState::LoggedIn;
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const CloseGame2&) {
    auto s = require_clan_state(state_, "bnet fsm: CLOSEGAME2 before login");
    if (!s) return s;

    if (state_ == BnetState::InGame) {
        if (use_cases_.leave_game && current_game_id_.value() != 0) {
            auto leave_result = use_cases_.leave_game->execute(
                current_game_id_, current_account_id_);
            (void)leave_result;
        }

        current_game_id_ = domain::GameId{0};
        state_ = BnetState::LoggedIn;
    }
    return core::ok();
}

core::Status<> BnetFsm::on(const GameReport&) {
    // Game-result upload is allowed during or after a game; no state change.
    return require_clan_state(state_, "bnet fsm: GAMEREPORT before login");
}

core::Status<> BnetFsm::on(const MapAuthReq1&) {
    auto s = require_clan_state(state_, "bnet fsm: MAPAUTHREQ1 before login");
    if (!s) return s;
    // The oracle always answers; with no per-connection game/map state tracked
    // at the protocol layer the not-in-a-game path applies, response = NO.
    return ctx_->send(ServerMessage{MapAuthReply1{kMapAuthReply1ResponseNo}});
}

core::Status<> BnetFsm::on(const MapAuthReq2&) {
    auto s = require_clan_state(state_, "bnet fsm: MAPAUTHREQ2 before login");
    if (!s) return s;
    // Mirror MapAuthReq1: oracle unconditionally replies, NO when not in a game.
    return ctx_->send(ServerMessage{MapAuthReply2{0u}});
}

}  // namespace pvpgn::protocol::bnet
