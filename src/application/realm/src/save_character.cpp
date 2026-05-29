// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/save_character.hpp"

namespace pvpgn::application::realm {

SaveCharacterUseCase::SaveCharacterUseCase(ICharacterRepository&          char_repo,
                                           domain::realm::ISaveFileStore& store)
    : char_repo_(char_repo)
    , store_(store)
{
}

core::Result<void, core::Error>
SaveCharacterUseCase::execute(const SaveCharacterCommand& cmd) {
    // Validate inputs
    if (cmd.account_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Account name cannot be empty"));
    }
    if (cmd.char_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Character name cannot be empty"));
    }
    if (cmd.save_data.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Save data cannot be empty"));
    }

    // Find the character
    domain::realm::CharacterId id{cmd.account_name, cmd.char_name};
    auto find_result = char_repo_.find(id);
    if (!find_result) {
        return core::fail(std::move(find_result).error());
    }

    const auto& character = find_result.value();

    // Only a locked character (one that is in a game) can be saved
    if (!character.is_locked()) {
        return core::fail(core::make_error(core::StatusCode::FailedPrecondition,
                                           "Cannot save an unlocked character"));
    }

    // Persist the save file
    return store_.store(cmd.account_name, cmd.char_name, cmd.save_data);
}

} // namespace pvpgn::application::realm
