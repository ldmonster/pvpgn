// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file create_character.hpp
/// Use case: create a new D2 character for an account.

#include "application/realm/character_lock.hpp"
#include "application/realm/character_list_repository.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include <string>

namespace pvpgn::application::realm {

/// Input command for creating a character.
struct CreateCharacterCommand {
    std::string account_name;
    std::string char_name;
    domain::realm::CharacterClass char_class = domain::realm::CharacterClass::amazon;
    bool expansion = false;
    bool hardcore  = false;
};

/// Creates a new character and adds it to the account's character list.
///
/// Invariants enforced:
///   - `account_name` and `char_name` must be non-empty.
///   - `char_name` must be at most 15 characters.
///   - The account must not already have 8 characters.
///   - No character with the same name may exist for the account.
class CreateCharacterUseCase {
public:
    CreateCharacterUseCase(ICharacterRepository&      char_repo,
                           ICharacterListRepository&  list_repo);

    core::Result<void, core::Error> execute(const CreateCharacterCommand& cmd);

private:
    ICharacterRepository&     char_repo_;
    ICharacterListRepository& list_repo_;
};

} // namespace pvpgn::application::realm
