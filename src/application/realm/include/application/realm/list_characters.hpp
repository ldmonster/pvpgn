// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file list_characters.hpp
/// Use case: list all characters for an account.

#include "application/realm/character_list_repository.hpp"
#include "core/result.hpp"
#include "domain/realm/character.hpp"
#include "domain/realm/character_list.hpp"
#include <string>
#include <vector>

namespace pvpgn::application::realm {

/// Summary view of a single character (no save-file data).
struct CharacterSummary {
    std::string                       char_name;
    domain::realm::CharacterClass     char_class = domain::realm::CharacterClass::amazon;
    uint32_t                          level      = 1;
    bool                              expansion  = false;
    bool                              hardcore   = false;
    bool                              dead       = false;
};

/// Input query for listing characters.
struct ListCharactersQuery {
    std::string                  account_name;
    domain::realm::SortMode      sort_mode = domain::realm::SortMode::by_name;
};

/// Result of listing characters.
struct ListCharactersResult {
    std::vector<CharacterSummary> characters;
};

/// Returns a sorted list of character summaries for the given account.
///
/// Invariants enforced:
///   - `account_name` must be non-empty.
class ListCharactersUseCase {
public:
    explicit ListCharactersUseCase(ICharacterListRepository& list_repo);

    core::Result<ListCharactersResult, core::Error>
    execute(const ListCharactersQuery& query);

private:
    ICharacterListRepository& list_repo_;
};

} // namespace pvpgn::application::realm
