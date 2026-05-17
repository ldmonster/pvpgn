// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file save_character.hpp
/// Use case: save character data received from a game server.

#include "application/realm/character_lock.hpp"
#include "core/result.hpp"
#include "domain/realm/dupe_checker.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::application::realm {

/// Input command for saving a character.
struct SaveCharacterCommand {
    std::string           account_name;
    std::string           char_name;
    std::vector<uint8_t>  save_data;
    std::string           gs_address;  ///< Game server that owns the lock
};

/// Saves character data from a game server.
///
/// Invariants enforced:
///   - `account_name` and `char_name` must be non-empty.
///   - `save_data` must be non-empty.
///   - The character must exist.
///   - The character must be locked (i.e. currently in a game).
class SaveCharacterUseCase {
public:
    SaveCharacterUseCase(ICharacterRepository&        char_repo,
                         domain::realm::ISaveFileStore& store);

    core::Result<void, core::Error> execute(const SaveCharacterCommand& cmd);

private:
    ICharacterRepository&          char_repo_;
    domain::realm::ISaveFileStore& store_;
};

} // namespace pvpgn::application::realm
