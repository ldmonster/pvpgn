// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "core/result.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace pvpgn::domain::ladder {
struct LadderEntry;
}

namespace pvpgn::application::ports {

/// Port: Ladder repository interface for hexagonal architecture.
class ILadderRepository {
public:
    virtual ~ILadderRepository() = default;

    /// Get rank of an account on the ladder.
    virtual core::Result<uint32_t, core::Error>
        get_rank(std::string_view account_name) = 0;

    /// Save or update a ladder entry.
    virtual core::Result<void, core::Error>
        save_entry(const domain::ladder::LadderEntry& entry) = 0;

    /// Get top N entries from the ladder.
    virtual core::Result<std::vector<domain::ladder::LadderEntry>, core::Error>
        get_top_n(uint32_t n) = 0;
};

} // namespace pvpgn::application::ports
