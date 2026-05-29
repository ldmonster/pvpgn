// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/join_game_server.hpp"

namespace pvpgn::application::realm {

JoinGameServerUseCase::JoinGameServerUseCase(ICharacterRepository& char_repo,
                                             GameServerQueue&       gs_queue)
    : char_repo_(char_repo)
    , gs_queue_(gs_queue)
{
}

core::Result<JoinGameServerResult, core::Error>
JoinGameServerUseCase::execute(const JoinGameServerCommand& cmd) {
    // Validate inputs
    if (cmd.account_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Account name cannot be empty"));
    }
    if (cmd.char_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Character name cannot be empty"));
    }

    // Find the character
    domain::realm::CharacterId id{cmd.account_name, cmd.char_name};
    auto find_result = char_repo_.find(id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }

    const auto& character = find_result.value();

    // Refuse to join if the character is already in a game
    if (character.is_locked()) {
        return core::fail(core::make_error(core::StatusCode::FailedPrecondition,
                                           "Character is already in a game"));
    }

    // Find the requested game server
    auto server_result = gs_queue_.find_server(cmd.gs_address);
    if (!server_result) {
        return core::fail(std::move(server_result).error());
    }

    const auto& server = server_result.value();

    // Create (or join) the game
    GameInfo game_info;
    game_info.game_name     = cmd.game_name;
    game_info.game_password = cmd.game_pass;
    game_info.gs_address    = server.address;
    game_info.gs_port       = server.port;

    auto token_result = gs_queue_.create_game(game_info);
    if (!token_result) {
        return core::fail(std::move(token_result).error());
    }

    JoinGameServerResult result;
    result.game_token = token_result.value();
    result.gs_address = server.address;
    result.gs_port    = server.port;

    return core::Result<JoinGameServerResult, core::Error>(std::move(result));
}

} // namespace pvpgn::application::realm
