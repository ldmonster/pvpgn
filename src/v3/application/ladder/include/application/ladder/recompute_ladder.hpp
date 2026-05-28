// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/// @file recompute_ladder.hpp
/// RECOMPUTE_LADDER use-case — re-rank all entries for a given ladder.
///
/// Fetches the top-N entries, sorts by wins (descending), assigns rank
/// positions, and persists the updated entries back to the repository.

#include <cstddef>
#include <string>

#include "core/error.hpp"
#include "core/result.hpp"

namespace pvpgn::application::ports {
class ILadderRepository;
}  // namespace pvpgn::application::ports

namespace pvpgn::application::ladder {

struct RecomputeLadderCommand {
    std::string ladder_id;  ///< e.g. "STAR", "WAR3", "W3XP"
};

struct RecomputeLadderResult {
    std::size_t entries_updated;
};

class RecomputeLadder {
public:
    explicit RecomputeLadder(ports::ILadderRepository& ladder)
        : ladder_(ladder) {}

    /// Returns InvalidArgument if ladder_id is empty.
    /// Returns NotFound if no entries exist for this ladder.
    [[nodiscard]] core::Status<RecomputeLadderResult>
    execute(RecomputeLadderCommand cmd) const;

private:
    ports::ILadderRepository& ladder_;
};

}  // namespace pvpgn::application::ladder
