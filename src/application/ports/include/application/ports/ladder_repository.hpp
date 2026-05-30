// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file ladder_repository.hpp
/// Application-layer port for the ladder / leaderboard store.

#include <cstdint>
#include <string_view>
#include <vector>

#include "core/error.hpp"
#include "core/result.hpp"
#include "domain/ladder/ladder.hpp"

namespace pvpgn::application::ports {

class ILadderRepository {
public:
    virtual ~ILadderRepository() = default;

    ILadderRepository(const ILadderRepository&)            = delete;
    ILadderRepository& operator=(const ILadderRepository&) = delete;
    ILadderRepository(ILadderRepository&&)                 = delete;
    ILadderRepository& operator=(ILadderRepository&&)      = delete;

    [[nodiscard]] virtual core::Result<std::uint32_t, core::Error>
    get_rank(std::string_view account_name) = 0;

    virtual core::Result<void, core::Error>
    save_entry(const domain::ladder::LadderEntry& entry) = 0;

    [[nodiscard]] virtual core::Result<std::vector<domain::ladder::LadderEntry>,
                                       core::Error>
    get_top_n(std::uint32_t n) = 0;

protected:
    ILadderRepository() = default;
};

} // namespace pvpgn::application::ports
