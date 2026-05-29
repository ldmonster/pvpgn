// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/load_character.hpp"

namespace pvpgn::application::realm {

LoadCharacterUseCase::LoadCharacterUseCase(ICharacterRepository&          char_repo,
                                           domain::realm::ISaveFileStore& store)
    : char_repo_(char_repo)
    , store_(store)
{
}

core::Result<LoadCharacterResult, core::Error>
LoadCharacterUseCase::execute(const LoadCharacterCommand& cmd) {
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

    // Refuse to load if the character is locked by a *different* server
    if (character.is_locked()) {
        const auto& locked_by = character.locked_by().value();
        if (locked_by != cmd.gs_address) {
            return core::fail(core::make_error(
                core::StatusCode::FailedPrecondition,
                "Character is locked by another game server: " + locked_by));
        }
    }

    // Load the save file
    auto load_result = store_.load(cmd.account_name, cmd.char_name);
    if (!load_result) {
        return core::fail(std::move(load_result).error());
    }

    LoadCharacterResult result;
    result.save_data = std::move(load_result).value();

    return core::Result<LoadCharacterResult, core::Error>(std::move(result));
}

} // namespace pvpgn::application::realm
