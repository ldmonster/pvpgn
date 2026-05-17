// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/delete_character.hpp"

namespace pvpgn::application::realm {

DeleteCharacterUseCase::DeleteCharacterUseCase(ICharacterRepository&     char_repo,
                                               ICharacterListRepository& list_repo)
    : char_repo_(char_repo)
    , list_repo_(list_repo)
{
}

core::Result<void, core::Error>
DeleteCharacterUseCase::execute(const DeleteCharacterCommand& cmd) {
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

    // Refuse to delete a character that is currently in a game
    if (character.is_locked()) {
        return core::fail(core::make_error(core::StatusCode::FailedPrecondition,
                                           "Cannot delete a character that is currently in a game"));
    }

    // Load the account's character list
    auto list_result = list_repo_.find_for_account(cmd.account_name);
    if (!list_result) {
        return core::fail(std::move(list_result).error());
    }
    auto char_list = std::move(list_result).value();

    // Remove from the list aggregate (best-effort; character may not be in list)
    auto remove_list_result = char_list.remove(cmd.char_name);
    (void)remove_list_result; // NotFound is acceptable here

    // Persist the updated list
    auto save_list_result = list_repo_.save(cmd.account_name, char_list);
    if (!save_list_result) {
        return core::fail(std::move(save_list_result).error());
    }

    // Remove the character record from the repository
    return char_repo_.remove(id);
}

} // namespace pvpgn::application::realm
