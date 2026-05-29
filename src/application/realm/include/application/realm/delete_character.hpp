// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file delete_character.hpp
/// Use case: delete a D2 character from an account.

#include "application/realm/character_lock.hpp"
#include "application/realm/character_list_repository.hpp"
#include "core/result.hpp"
#include <string>

namespace pvpgn::application::realm {

/// Input command for deleting a character.
struct DeleteCharacterCommand {
    std::string account_name;
    std::string char_name;
};

/// Deletes a character from the account's character list.
///
/// Invariants enforced:
///   - `account_name` and `char_name` must be non-empty.
///   - The character must exist.
///   - The character must not be currently locked (in a game).
class DeleteCharacterUseCase {
public:
    DeleteCharacterUseCase(ICharacterRepository&     char_repo,
                           ICharacterListRepository& list_repo);

    core::Result<void, core::Error> execute(const DeleteCharacterCommand& cmd);

private:
    ICharacterRepository&     char_repo_;
    ICharacterListRepository& list_repo_;
};

} // namespace pvpgn::application::realm
