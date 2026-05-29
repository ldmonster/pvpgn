// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file load_character.hpp
/// Use case: load character save data for a game server.

#include "application/realm/character_lock.hpp"
#include "core/result.hpp"
#include "domain/realm/dupe_checker.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace pvpgn::application::realm {

/// Input command for loading a character.
struct LoadCharacterCommand {
    std::string account_name;
    std::string char_name;
    std::string gs_address;  ///< Game server requesting the load
};

/// Result returned on successful load.
struct LoadCharacterResult {
    std::vector<uint8_t> save_data;
};

/// Loads character save data for a game server.
///
/// Invariants enforced:
///   - `account_name` and `char_name` must be non-empty.
///   - The character must exist.
///   - The character must not be locked by a *different* server.
///     (A character locked by the same server, or unlocked, can be loaded.)
class LoadCharacterUseCase {
public:
    LoadCharacterUseCase(ICharacterRepository&          char_repo,
                         domain::realm::ISaveFileStore& store);

    core::Result<LoadCharacterResult, core::Error>
    execute(const LoadCharacterCommand& cmd);

private:
    ICharacterRepository&          char_repo_;
    domain::realm::ISaveFileStore& store_;
};

} // namespace pvpgn::application::realm
