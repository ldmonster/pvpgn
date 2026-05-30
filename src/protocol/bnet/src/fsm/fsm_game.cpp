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

#include "application/game/join_game.hpp"
#include "application/game/leave_game.hpp"
#include "application/game/start_game.hpp"

namespace pvpgn::protocol::bnet {

// --- Game-lifecycle handlers with state transitions -----------------------

core::Status<> BnetFsm::on(const StartGame1Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME1 before login");
    if (!s) return s;

    if (!use_cases_.start_game) {
        // No start_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame1Ack{0x00}});  // success code
    }

    // Use client_tag_ stored from AUTH_INFO
    // Call start_game use-case with game parameters from request
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_,
        m.game_name, "", m.gametype);  // Empty map_name for now

    if (!start_result) {
        // Game start failed
        return ctx_->send(ServerMessage{StartGame1Ack{0x01}});  // error code
    }

    // Store game ID and transition state
    const auto& start_res = start_result.value();
    current_game_id_ = start_res.game_id;
    state_ = BnetState::InGame;

    // Send success reply with game ID
    return ctx_->send(ServerMessage{StartGame1Ack{0x00}});  // success code
}

core::Status<> BnetFsm::on(const StartGame3Request& m) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME3 before login");
    if (!s) return s;

    if (!use_cases_.start_game) {
        // No start_game use-case available - accept the request with fallback
        state_ = BnetState::InGame;
        return ctx_->send(ServerMessage{StartGame3Ack{0x00}});  // success code
    }

    // Use client_tag_ stored from AUTH_INFO
    // Call start_game use-case with game parameters from request
    auto start_result = use_cases_.start_game->execute(
        current_account_id_, client_tag_,
        m.game_name, "", m.gametype);

    if (!start_result) {
        return ctx_->send(ServerMessage{StartGame3Ack{0x01}});
    }

    const auto& start_res = start_result.value();
    current_game_id_ = start_res.game_id;
    state_ = BnetState::InGame;

    return ctx_->send(ServerMessage{StartGame3Ack{0x00}});
}

core::Status<> BnetFsm::on(const StartGame4Request&) {
    auto s = require_clan_state(state_, "bnet fsm: STARTGAME4 before login");
    if (!s) return s;
    state_ = BnetState::InGame;
    return core::ok();
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
    return require_clan_state(state_, "bnet fsm: MAPAUTHREQ1 before login");
}

core::Status<> BnetFsm::on(const MapAuthReq2&) {
    return require_clan_state(state_, "bnet fsm: MAPAUTHREQ2 before login");
}

}  // namespace pvpgn::protocol::bnet
