// SPDX-License-Identifier: GPL-2.0-or-later
#include "application/realm/create_character.hpp"

namespace pvpgn::application::realm {

namespace {
    constexpr std::size_t kMaxCharNameLength = 15;
} // namespace

CreateCharacterUseCase::CreateCharacterUseCase(ICharacterRepository&     char_repo,
                                               ICharacterListRepository& list_repo)
    : char_repo_(char_repo)
    , list_repo_(list_repo)
{
}

core::Result<void, core::Error>
CreateCharacterUseCase::execute(const CreateCharacterCommand& cmd) {
    // Validate inputs
    if (cmd.account_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Account name cannot be empty"));
    }
    if (cmd.char_name.empty()) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Character name cannot be empty"));
    }
    if (cmd.char_name.size() > kMaxCharNameLength) {
        return core::fail(core::make_error(core::StatusCode::InvalidArgument,
                                           "Character name exceeds maximum length of 15"));
    }

    // Load the account's character list
    auto list_result = list_repo_.find_for_account(cmd.account_name);
    if (!list_result) {
        return core::fail(std::move(list_result).error());
    }
    auto char_list = std::move(list_result).value();

    // Check capacity
    if (char_list.is_full()) {
        return core::fail(core::make_error(core::StatusCode::ResourceExhausted,
                                           "Account has reached the maximum number of characters"));
    }

    // Check for duplicate name
    auto find_existing = char_list.find(cmd.char_name);
    if (find_existing.has_value()) {
        return core::fail(core::make_error(core::StatusCode::AlreadyExists,
                                           "A character with that name already exists"));
    }

    // Build the new character
    domain::realm::CharacterId id{cmd.account_name, cmd.char_name};
    domain::realm::CharacterStats stats;
    stats.char_class = cmd.char_class;
    stats.expansion  = cmd.expansion
                           ? domain::realm::CharacterExpansion::lod
                           : domain::realm::CharacterExpansion::classic;
    stats.hardcore   = cmd.hardcore
                           ? domain::realm::CharacterHardcore::hardcore
                           : domain::realm::CharacterHardcore::softcore;

    domain::realm::Character character{id, stats};

    // Add to the list aggregate
    auto add_result = char_list.add(character);
    if (!add_result) {
        return core::fail(std::move(add_result).error());
    }

    // Persist the character
    auto save_char_result = char_repo_.save(character);
    if (!save_char_result) {
        return core::fail(std::move(save_char_result).error());
    }

    // Persist the updated list
    return list_repo_.save(cmd.account_name, char_list);
}

} // namespace pvpgn::application::realm
