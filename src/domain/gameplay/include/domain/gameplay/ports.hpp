// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
//
// domain/gameplay/ports.hpp — Abstract ports (interfaces) for the gameplay bounded context.
// Implementations live in src/infra/<tech>/ and src/integration/<binding>/.

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/gameplay/game.hpp"

namespace pvpgn::domain::gameplay {

// ---------------------------------------------------------------------------
// IGameRepository
// ---------------------------------------------------------------------------

class IGameRepository {
public:
    virtual ~IGameRepository() = default;

    IGameRepository(const IGameRepository&)            = delete;
    IGameRepository& operator=(const IGameRepository&) = delete;
    IGameRepository(IGameRepository&&)                 = delete;
    IGameRepository& operator=(IGameRepository&&)      = delete;

    /// Lookup a game by its display name.
    [[nodiscard]] virtual core::Result<std::shared_ptr<Game>,
                                       core::Error>
    find_by_name(std::string_view name) = 0;

    /// Lookup a game by its numeric id.
    [[nodiscard]] virtual core::Result<std::shared_ptr<Game>,
                                       core::Error>
    find_by_id(std::uint32_t id) = 0;

    /// Insert-or-update @p game.
    virtual core::Result<void, core::Error>
    save(const Game& game) = 0;

    /// Remove the game whose display name is @p name.
    virtual core::Result<void, core::Error>
    remove(std::string_view name) = 0;

    /// Snapshot of every currently-tracked game.
    [[nodiscard]] virtual core::Result<
        std::vector<std::shared_ptr<Game>>, core::Error>
    list_active() = 0;

protected:
    IGameRepository() = default;
};

} // namespace pvpgn::domain::gameplay
