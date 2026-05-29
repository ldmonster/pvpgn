// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file character_list_repository.hpp
/// Port interface for persisting `CharacterList` aggregates.

#include "core/result.hpp"
#include "domain/realm/character_list.hpp"
#include <string_view>

namespace pvpgn::application::realm {

/// Port: persistence for per-account character lists.
class ICharacterListRepository {
public:
    virtual ~ICharacterListRepository() = default;

    /// Load the character list for the given account.
    /// Returns an empty `CharacterList` (not an error) when the account has
    /// no characters yet.
    virtual core::Result<domain::realm::CharacterList, core::Error>
    find_for_account(std::string_view account_name) = 0;

    /// Persist the character list for the given account.
    virtual core::Result<void, core::Error>
    save(std::string_view account_name,
         const domain::realm::CharacterList& list) = 0;
};

} // namespace pvpgn::application::realm
