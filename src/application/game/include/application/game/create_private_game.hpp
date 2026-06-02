// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file create_private_game.hpp
/// CREATE_PRIVATE_GAME use-case — host creates a password-protected game.
///
/// Like StartGame but with password protection. The game is created
/// and automatically started (not held in Open state).

#include <memory>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/shared/client_tag.hpp"
#include "domain/shared/ids.hpp"
#include "domain/gameplay/ports.hpp"
#include "domain/shared/event_bus.hpp"

namespace pvpgn::application::game {

struct CreatePrivateGameRequest {
    domain::AccountId host_id;
    std::string       game_name;
    std::string       password;      // non-empty for private
    domain::ClientTag client_tag;
    std::string       map_name;
    std::uint32_t     max_players{8};
};

enum class CreatePrivateGameError : std::uint8_t {
    InvalidGameName,
    InvalidPassword,
    MaxPlayersOutOfRange,
    PersistenceFailed,
    Internal,
};

class CreatePrivateGame {
public:
    CreatePrivateGame(std::shared_ptr<domain::gameplay::IGameRepository> games,
                      std::shared_ptr<application::ports::IEventBus> event_bus)
        : games_(games), event_bus_(event_bus) {}

    core::Result<domain::GameId, CreatePrivateGameError>
    execute(const CreatePrivateGameRequest& req);

private:
    std::shared_ptr<domain::gameplay::IGameRepository> games_;
    std::shared_ptr<application::ports::IEventBus> event_bus_;
};

}  // namespace pvpgn::application::game
