// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file join_game_server.hpp
/// Use case: join (or create) a game on a game server.

#include "application/realm/character_lock.hpp"
#include "application/realm/gs_queue.hpp"
#include "core/result.hpp"
#include <cstdint>
#include <string>

namespace pvpgn::application::realm {

/// Input command for joining a game server.
struct JoinGameServerCommand {
    std::string account_name;
    std::string char_name;
    std::string game_name;
    std::string game_pass;
    std::string gs_address;  ///< Requested game server address
};

/// Result returned on successful join.
struct JoinGameServerResult {
    uint32_t    game_token = 0;
    std::string gs_address;
    uint16_t    gs_port    = 0;
};

/// Joins (or creates) a game for the given character on the given game server.
///
/// Invariants enforced:
///   - `account_name` and `char_name` must be non-empty.
///   - The character must exist.
///   - The character must not already be locked (in another game).
///   - The requested game server must exist and be online.
class JoinGameServerUseCase {
public:
    JoinGameServerUseCase(ICharacterRepository& char_repo,
                          GameServerQueue&       gs_queue);

    core::Result<JoinGameServerResult, core::Error>
    execute(const JoinGameServerCommand& cmd);

private:
    ICharacterRepository& char_repo_;
    GameServerQueue&       gs_queue_;
};

} // namespace pvpgn::application::realm
